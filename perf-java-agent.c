/*
 * libperfjava: JVM agent to create a perf-<pid>.map file for consumption
 *              using the Linux perf tools
 *
 * Copyright (C) 2015 Jesper Pedersen <jesper.pedersen@comcast.net>
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
#include <stdio.h>
#include <string.h>

#include <sys/types.h>

#include <jni.h>
#include <jvmti.h>
#include <jvmticmlr.h>

#include "perf-map-file.h"

FILE *file = NULL;

static void
signature_string(jvmtiEnv *jvmti, jmethodID method, char *output, size_t noutput)
{
   char *name;
   char *msig;
   jclass class;
   char *csig;

   (*jvmti)->GetMethodName(jvmti, method, &name, &msig, NULL);
   (*jvmti)->GetMethodDeclaringClass(jvmti, method, &class);
   (*jvmti)->GetClassSignature(jvmti, class, &csig, NULL);

   snprintf(output, noutput, "%s.%s%s", csig, name, msig);

   (*jvmti)->Deallocate(jvmti, name);
   (*jvmti)->Deallocate(jvmti, msig);
   (*jvmti)->Deallocate(jvmti, csig);
}

void
generate_single_entry(jvmtiEnv *jvmti, jmethodID method, const void *code_addr, jint code_size)
{
   char entry[200];
   signature_string(jvmti, method, entry, sizeof(entry));
   perf_map_write_entry(file, code_addr, code_size, entry);
}

static void JNICALL
callbackCompiledMethodLoad(jvmtiEnv *jvmti, jmethodID method, jint code_size, const void* code_addr,
                           jint map_length, const jvmtiAddrLocationMap* map, const void* compile_info)
{
   generate_single_entry(jvmti, method, code_addr, code_size);
}

void JNICALL
callbackDynamicCodeGenerated(jvmtiEnv *jvmti, const char* name, const void* address, jint length)
{
   perf_map_write_entry(file, address, length, name);
}

jvmtiError
enable_capabilities(jvmtiEnv *jvmti)
{
   jvmtiCapabilities capabilities;

   memset(&capabilities,0, sizeof(capabilities));
   capabilities.can_generate_all_class_hook_events  = 1;
   capabilities.can_tag_objects                     = 1;
   capabilities.can_generate_object_free_events     = 1;
   capabilities.can_get_source_file_name            = 1;
   capabilities.can_get_line_numbers                = 1;
   capabilities.can_generate_vm_object_alloc_events = 1;
   capabilities.can_generate_compiled_method_load_events = 1;

   // Request these capabilities for this JVM TI environment.
   return (*jvmti)->AddCapabilities(jvmti, &capabilities);
}

jvmtiError
set_callbacks(jvmtiEnv *jvmti)
{
   jvmtiEventCallbacks callbacks;

   memset(&callbacks, 0, sizeof(callbacks));
   callbacks.CompiledMethodLoad  = &callbackCompiledMethodLoad;
   callbacks.DynamicCodeGenerated = &callbackDynamicCodeGenerated;
   return (*jvmti)->SetEventCallbacks(jvmti, &callbacks, (jint)sizeof(callbacks));
}

JNIEXPORT jint JNICALL 
Agent_OnLoad(JavaVM *vm, char *options, void *reserved)
{
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
