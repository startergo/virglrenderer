/*
 * Copyright 2024 Qualcomm Innovation Center, Inc. All Rights Reserved.
 * SPDX-License-Identifier: MIT
 */

#include "vcomp_cl_context.h"
#include "vcomp_common.h"
#include "vcomp_context.h"
#include "vcomp_event.h"
#include "vcomp_queue.h"

#include "vcl-protocol/vcl_protocol_renderer_defines.h"

struct vcomp_event
{
   struct vcomp_object base;
};
VCOMP_DEFINE_OBJECT_CAST(event, cl_event)

void
vcomp_context_add_event(struct vcomp_context *vctx, cl_event event,
                        cl_event *args_event, cl_int *args_ret)
{
   const vcomp_object_id id = vcomp_cs_handle_load_id((const void **)args_event);
   if (!vcomp_context_validate_object_id(vctx, id))
   {
      *args_ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   struct vcomp_event *v_event = vcomp_object_alloc(sizeof(*v_event), id);
   if (!v_event)
   {
      *args_ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   v_event->base.handle.event = event;

   vcomp_context_add_object(vctx, &v_event->base);
}

static void
vcomp_dispatch_clCreateUserEventMESA(struct vcl_dispatch_context *dispatch,
                                     struct vcl_command_clCreateUserEventMESA *args)
{
   struct vcomp_context *vctx = dispatch->data;

   struct vcomp_cl_context *context = vcomp_cl_context_from_handle(args->context);
   if (!context)
   {
      args->ret = CL_INVALID_CONTEXT;
      return;
   }

   cl_event event = clCreateUserEvent(context->base.handle.cl_context,
                                      &args->ret);
   if (!event)
   {
      args->ret = CL_OUT_OF_HOST_MEMORY;
      return;
   }

   vcomp_context_add_event(vctx, event, args->event, &args->ret);
}

static void
vcomp_dispatch_clSetUserEventStatus(UNUSED struct vcl_dispatch_context *dispatch,
                                    struct vcl_command_clSetUserEventStatus *args)
{
   struct vcomp_event *event = vcomp_event_from_handle(args->event);
   if (!event)
   {
      args->ret = CL_INVALID_EVENT;
      return;
   }

   args->ret = clSetUserEventStatus(event->base.handle.event,
                                    args->execution_status);
}

static void
vcomp_dispatch_clWaitForEvents(UNUSED struct vcl_dispatch_context *dispatch,
                               struct vcl_command_clWaitForEvents *args)
{
   cl_event *handles = calloc(args->num_events, sizeof(*handles));
   for (uint32_t i = 0; i < args->num_events; ++i)
   {
      struct vcomp_event *event = vcomp_event_from_handle(args->event_list[i]);
      if (!event)
      {
         args->ret = CL_INVALID_EVENT;
         goto free_handles;
      }
      handles[i] = event->base.handle.event;
   }

   args->ret = clWaitForEvents(args->num_events, handles);

free_handles:
   free(handles);
}

static void
vcomp_dispatch_clGetEventInfo(UNUSED struct vcl_dispatch_context *dispatch,
                              struct vcl_command_clGetEventInfo *args)
{
   struct vcomp_event *event = vcomp_event_from_handle(args->event);
   if (!event)
   {
      args->ret = CL_INVALID_EVENT;
      return;
   }

   args->ret = clGetEventInfo(event->base.handle.event, args->param_name,
                              args->param_value_size, args->param_value,
                              args->param_value_size_ret);
}

static void
vcomp_dispatch_clReleaseEvent(struct vcl_dispatch_context *dispatch,
                              struct vcl_command_clReleaseEvent *args)
{
   struct vcomp_context *vctx = dispatch->data;

   struct vcomp_event *event = vcomp_event_from_handle(args->event);
   if (!event)
   {
      args->ret = CL_INVALID_EVENT;
      return;
   }

   args->ret = clReleaseEvent(event->base.handle.event);

   vcomp_context_remove_object(vctx, &event->base);
}

static void
vcomp_dispatch_clSetEventCallback(UNUSED struct vcl_dispatch_context *dispatch,
                                  UNUSED struct vcl_command_clSetEventCallback *args)
{
}

static void
vcomp_dispatch_clEnqueueMarkerWithWaitList(struct vcl_dispatch_context *dispatch,
                                           struct vcl_command_clEnqueueMarkerWithWaitList *args)
{
   struct vcomp_context *vctx = dispatch->data;

   struct vcomp_queue *queue = vcomp_queue_from_handle(args->command_queue);
   if (!queue)
   {
      args->ret = CL_INVALID_COMMAND_QUEUE;
      return;
   }

   cl_event *handles = calloc(args->num_events_in_wait_list, sizeof(*handles));
   for (uint32_t i = 0; i < args->num_events_in_wait_list; ++i)
   {
      struct vcomp_event *event = vcomp_event_from_handle(args->event_wait_list[i]);
      if (!event)
      {
         args->ret = CL_INVALID_EVENT_WAIT_LIST;
         goto free_handles;
      }
      handles[i] = event->base.handle.event;
   }

   args->ret = clEnqueueMarkerWithWaitList(queue->base.handle.queue,
                                           args->num_events_in_wait_list,
                                           handles, args->event);

   if (args->event) {
      vcomp_context_add_event(vctx, *args->event, args->event, &args->ret);
   }

free_handles:
   free(handles);
}

static void
vcomp_dispatch_clEnqueueBarrierWithWaitList(struct vcl_dispatch_context *dispatch,
                                            struct vcl_command_clEnqueueBarrierWithWaitList *args)
{
   struct vcomp_context *vctx = dispatch->data;

   struct vcomp_queue *queue = vcomp_queue_from_handle(args->command_queue);
   if (!queue)
   {
      args->ret = CL_INVALID_COMMAND_QUEUE;
   }

   cl_event *handles = calloc(args->num_events_in_wait_list, sizeof(*handles));
   for (uint32_t i = 0; i < args->num_events_in_wait_list; ++i)
   {
      struct vcomp_event *event = vcomp_event_from_handle(args->event_wait_list[i]);
      if (!event)
      {
         args->ret = CL_INVALID_EVENT_WAIT_LIST;
         goto free_handles;
      }
      handles[i] = event->base.handle.event;
   }

   args->ret = clEnqueueBarrierWithWaitList(queue->base.handle.queue,
                                            args->num_events_in_wait_list,
                                            handles, args->event);

   if (args->event) {
      vcomp_context_add_event(vctx, *args->event, args->event, &args->ret);
   }

free_handles:
   free(handles);
}

static void
vcomp_dispatch_clGetEventProfilingInfo(UNUSED struct vcl_dispatch_context *dispatch,
                                       struct vcl_command_clGetEventProfilingInfo *args)
{
   struct vcomp_event *event = vcomp_event_from_handle(args->event);
   if (!event)
   {
      args->ret = CL_INVALID_EVENT;
      return;
   }

   args->ret = clGetEventProfilingInfo(event->base.handle.event,
                                       args->param_name,
                                       args->param_value_size,
                                       args->param_value,
                                       args->param_value_size_ret);
}

void
vcomp_context_init_event_dispatch(struct vcomp_context *vctx)
{
   struct vcl_dispatch_context *dispatch = &vctx->dispatch;

   dispatch->dispatch_clCreateUserEventMESA = vcomp_dispatch_clCreateUserEventMESA;
   dispatch->dispatch_clSetUserEventStatus = vcomp_dispatch_clSetUserEventStatus;
   dispatch->dispatch_clWaitForEvents = vcomp_dispatch_clWaitForEvents;
   dispatch->dispatch_clGetEventInfo = vcomp_dispatch_clGetEventInfo;
   dispatch->dispatch_clReleaseEvent = vcomp_dispatch_clReleaseEvent;
   dispatch->dispatch_clSetEventCallback = vcomp_dispatch_clSetEventCallback;
   dispatch->dispatch_clEnqueueMarkerWithWaitList = vcomp_dispatch_clEnqueueMarkerWithWaitList;
   dispatch->dispatch_clEnqueueBarrierWithWaitList = vcomp_dispatch_clEnqueueBarrierWithWaitList;
   dispatch->dispatch_clGetEventProfilingInfo = vcomp_dispatch_clGetEventProfilingInfo;
}
