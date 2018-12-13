/*
 * libperfjava: JVM agent to create a perf-<pid>.map file for consumption
 *              using the Linux perf tools
 *
 * Copyright (C) 2018 Jesper Pedersen <jesper.pedersen@comcast.net>
 *
 * Based on http://github.com/jrudolph/perf-map-agent
 * Copyright (C) 2013 Johannes Rudolph <johannes.rudolph@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>

#include <jni.h>
#include <jvmti.h>
#include <jvmticmlr.h>

#include "perf-map-file.h"

#define BUFFER_SIZE 512
#define BIG_BUFFER_SIZE 20480

FILE *file = NULL;
int flush = 0;
int unfold = 0;

static void
signature_string_f(jvmtiEnv *jvmti, jmethodID method, char *output, size_t noutput, char* annotation)
{
   char *name;
   char *msig;
   jclass class;
   char *csig;

   (*jvmti)->GetMethodName(jvmti, method, &name, &msig, NULL);
   (*jvmti)->GetMethodDeclaringClass(jvmti, method, &class);
   (*jvmti)->GetClassSignature(jvmti, class, &csig, NULL);

   if (annotation)
      snprintf(output, noutput, "%s.%s%s", csig, name, annotation);
   else
      snprintf(output, noutput, "%s.%s", csig, name);

   (*jvmti)->Deallocate(jvmti, name);
   (*jvmti)->Deallocate(jvmti, msig);
   (*jvmti)->Deallocate(jvmti, csig);
}

static void
signature_string(jvmtiEnv *jvmti, jmethodID method, char *output, size_t noutput)
{
   signature_string_f(jvmti, method, output, noutput, NULL);
}

static char *
frame_annotation(int inlined)
{
   return inlined ? "_[i]" : "_[j]";
}

static void
write_unfolded_entry(jvmtiEnv *jvmti, PCStackInfo *info, jmethodID root_method,
                     const char *root_name, const void *start_addr, const void *end_addr)
{
   char inlined_name[BUFFER_SIZE];
   const char *entry_p;

   if (unfold)
   {
      char full_name[BIG_BUFFER_SIZE];
      full_name[0] = '\0';

      const jint first_frame = info->numstackframes - 1;
      for (int i = first_frame; i >= 0; i--)
      {
         signature_string_f(jvmti, info->methods[i], inlined_name, sizeof(inlined_name), frame_annotation(i != first_frame));
         strncat(full_name, inlined_name, sizeof(full_name) - 1 - strlen(full_name));
         if (i != 0)
            strncat(full_name, "->", sizeof(full_name) - 1 - strlen(full_name));
      }
      entry_p = full_name;
   }
   else
   {
      jmethodID cur_method = info->methods[0];
      if (cur_method != root_method)
      {
         signature_string(jvmti, cur_method, inlined_name, sizeof(inlined_name));
         entry_p = inlined_name;
      }
      else
      {
         entry_p = root_name;
      }
   }

   perf_map_write_entry(file, start_addr, (unsigned int) (end_addr - start_addr), entry_p, flush);
}

static void
generate_single_entry(jvmtiEnv *jvmti, jmethodID method, const void *code_addr, jint code_size)
{
   char entry[BUFFER_SIZE];
   signature_string(jvmti, method, entry, sizeof(entry));
   perf_map_write_entry(file, code_addr, code_size, entry, flush);
}

static void
generate_unfolded_entries(jvmtiEnv *jvmti, jmethodID root_method, jint code_size,
                          const void* code_addr, const void* compile_info)
{
   const jvmtiCompiledMethodLoadRecordHeader *header = (jvmtiCompiledMethodLoadRecordHeader *) compile_info;
   char root_name[BUFFER_SIZE];
   int i;

   signature_string(jvmti, root_method, root_name, sizeof(root_name));

   if (header->kind == JVMTI_CMLR_INLINE_INFO)
   {
      const jvmtiCompiledMethodLoadInlineRecord *record = (jvmtiCompiledMethodLoadInlineRecord *) header;

      const void *start_addr = code_addr;
      jmethodID cur_method = root_method;

      for (i = 0; i < record->numpcs; i++)
      {
         PCStackInfo *info = &record->pcinfo[i];
         jmethodID top_method = info->methods[0];

         if (cur_method != top_method)
         {
            void *end_addr = info->pc;

            if (i > 0)
               write_unfolded_entry(jvmti, &record->pcinfo[i - 1], root_method, root_name, start_addr, end_addr);
            else
               generate_single_entry(jvmti, root_method, start_addr, (unsigned int) (end_addr - start_addr));

            start_addr = info->pc;
            cur_method = top_method;
         }
      }

      if (start_addr != code_addr + code_size)
      {
         const void *end_addr = code_addr + code_size;

         if (i > 0)
            write_unfolded_entry(jvmti, &record->pcinfo[i - 1], root_method, root_name, start_addr, end_addr);
         else
            generate_single_entry(jvmti, root_method, start_addr, (unsigned int) (end_addr - start_addr));
      }
   }
   else
   {
      generate_single_entry(jvmti, root_method, code_addr, code_size);
   }
}

static void JNICALL
callbackCompiledMethodLoad(jvmtiEnv *jvmti, jmethodID method, jint code_size, const void* code_addr,
                           jint map_length, const jvmtiAddrLocationMap* map, const void* compile_info)
{
   if (compile_info != NULL)
      generate_unfolded_entries(jvmti, method, code_size, code_addr, compile_info);
   else
      generate_single_entry(jvmti, method, code_addr, code_size);
}

static void JNICALL
callbackDynamicCodeGenerated(jvmtiEnv *jvmti, const char* name, const void* address, jint length)
{
   perf_map_write_entry(file, address, length, name, flush);
}

static jvmtiError
enable_capabilities(jvmtiEnv *jvmti)
{
   jvmtiCapabilities capabilities;

   memset(&capabilities,0, sizeof(capabilities));
   capabilities.can_generate_all_class_hook_events  = 1;
   capabilities.can_tag_objects                     = 1;
   capabilities.can_generate_object_free_events     = 1;
   capabilities.can_get_source_file_name            = 1;
   capabilities.can_get_line_numbers                = 0;
   capabilities.can_generate_vm_object_alloc_events = 1;
   capabilities.can_generate_compiled_method_load_events = 1;

   // Request these capabilities for this JVM TI environment.
   return (*jvmti)->AddCapabilities(jvmti, &capabilities);
}

static jvmtiError
set_callbacks(jvmtiEnv *jvmti)
{
   jvmtiEventCallbacks callbacks;

   memset(&callbacks, 0, sizeof(callbacks));
   callbacks.CompiledMethodLoad  = &callbackCompiledMethodLoad;
   callbacks.DynamicCodeGenerated = &callbackDynamicCodeGenerated;
   return (*jvmti)->SetEventCallbacks(jvmti, &callbacks, (jint)sizeof(callbacks));
}

static void
option_flush(char* option)
{
   char* equal = strchr(option, '=');
   if (equal != NULL)
   {
      flush = atol(equal + 1);
   }
}

static void
option_unfold(char* option)
{
   char* equal = strchr(option, '=');
   if (equal != NULL)
   {
      unfold = atol(equal + 1);
   }
}

JNIEXPORT jint JNICALL 
Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
   if (options != NULL)
   {
      char* token = strtok(options, ",");
      while (token != NULL)
      {
         if (strstr(token, "flush") != NULL)
         {
            option_flush(token);
         }
         else if (strstr(token, "unfold") != NULL)
         {
            option_unfold(token);
         }
         token = strtok(NULL, ",");
      }
   }

   file = perf_map_open(getpid());

   jvmtiEnv *jvmti;
   (*vm)->GetEnv(vm, (void **)&jvmti, JVMTI_VERSION_1);
   enable_capabilities(jvmti);
   set_callbacks(jvmti);

   (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                      JVMTI_EVENT_COMPILED_METHOD_LOAD, (jthread)NULL);
   (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                      JVMTI_EVENT_DYNAMIC_CODE_GENERATED, (jthread)NULL);

   
   (*jvmti)->GenerateEvents(jvmti, JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
   (*jvmti)->GenerateEvents(jvmti, JVMTI_EVENT_COMPILED_METHOD_LOAD);

   return 0;
}

JNIEXPORT void JNICALL 
Agent_OnUnload(JavaVM *vm)
{
   perf_map_close(file);
   file = NULL;
}
