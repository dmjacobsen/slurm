/*****************************************************************************\
 *  shr_64.c - This plug is used by POE to interact with SLURM.
 *
 *****************************************************************************
 *  Copyright (C) 2012 SchedMD LLC.
 *  Written by Danny Auble <da@schedmd.com> et. al.
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.schedmd.com/slurmdocs/>.
 *  Please also read the included file: DISCLAIMER.
 *
 *  SLURM is free software; you can redistribute it and/or modify it under
 *  the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of portions of this program with the OpenSSL library under
 *  certain conditions as described in each individual source file, and
 *  distribute linked combinations including the two. You must obey the GNU
 *  General Public License in all respects for all of the code used other than
 *  OpenSSL. If you modify file(s) with this exception, you may extend this
 *  exception to your version of the file(s), but you are not obligated to do
 *  so. If you do not wish to do so, delete this exception statement from your
 *  version.  If you delete this exception statement from all source files in
 *  the program, then also delete it here.
 *
 *  SLURM is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with SLURM; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA.
\*****************************************************************************/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#include "src/common/xmalloc.h"
#include "src/common/list.h"
#include "src/common/hostlist.h"
#include <slurm/slurm.h>

#include <permapi.h>

/* The connection communicates information to and from the resource
 * manager, so that the resource manager can start the parallel task
 * manager, and is available for the caller to communicate directly
 * with the parallel task manager.
 * IN resource_mgr - The resource manager handle returned by pe_rm_init.
 * IN connect_param - Input parameter structure (rm_connect_param)
 *        that contains the following:
 *        machine_count: The count of hosts/machines.
 *        machine_name: The array of machine names on which to connect.
 *        executable: The name of the executable to be started.
 * IN rm_timeout - The integer value that defines a connection timeout
 *        value. This value is defined by the MP_RM_TIMEOUT
 *        environment variable. A value less than zero indicates there
 *        is no timeout. A value equal to zero means to immediately
 *        return with no wait or retry. A value greater than zero
 *        means to wait the specified amount of time (in seconds).
 * OUT rm_sockfds - An array of socket file descriptors, that are
 *        allocated by the caller, to be returned as output, of the connection.
 * OUT error_msg - An error message that explains the error.
 * RET 0 - SUCCESS, nonzero on failure.
 */
extern int pe_rm_connect(rmhandle_t resource_mgr,
			 rm_connect_param *connect_param,
			 int *rm_sockfds, int rm_timeout, char **error_msg)
{
	return 0;
}

/* Releases the resource manager handle, closes the socket that is
 * created by the pe_rm_init function, and releases memory
 * allocated. When called, pe_rm_free implies the job has completed
 * and resources are freed and available for subsequent jobs.
 * IN/OUT resource_mgr
 */
extern void pe_rm_free(rmhandle_t *resource_mgr)
{

}

/* The memory that is allocated to events generated by the resource
 * manager is released. pe_rm_free_event must be called for every
 * event that is received from the resource manager by calling the
 * pe_rm_get_event function.
 * IN resource_mgr
 * IN job_event - The pointer to a job event. The event must have been
 *        built by calling the pe_rm_get_event function.
 * RET 0 - SUCCESS, nonzero on failure.
 */
extern int pe_rm_free_event(rmhandle_t resource_mgr, job_event_t ** job_event)
{
	return 0;
}

/* This resource management interface is called to return job event
 * information. The pe_rm_get_event function is only called in
 * interactive mode.
 *
 * With interactive jobs, this function reads or selects on the listen
 * socket created by the pe_rm_init call. If the listen socket is not
 * ready to read, this function selects and waits. POE processes
 * should monitor this socket at all times for event notification from
 * the resource manager after the job has started running.
 *
 * This function returns a pointer to the event that was updated by
 * the transaction.
 * The valid events are:
 * JOB_ERROR_EVENT
 *        Job error messages occurred. In this case, POE displays the
 *        error and terminates.
 * JOB_STATE_EVENT
 *        A job status change occurred, which results in one of the
 *        following job states. In this case, the caller may need to take
 *        appropriate action.
 * JOB_STATE_RUNNING
 *        Indicates that the job has started. POE uses the
 *        pe_rm_get_job_info function to return the job
 *        information. When a job state of JOB_STATE_RUNNING has been
 *        returned, the job has started running and POE can obtain the
 *        job information by way of the pe_rm_get_job_info function call.
 * JOB_STATE_NOTRUN
 *        Indicates that the job was not run, and POE will terminate.
 * JOB_STATE_PREEMPTED
 *        Indicates that the job was preempted.
 * JOB_STATE_RESUMED
 *        Indicates that the job has resumed.
 * JOB_TIMER_EVENT
 *        Indicates that no events occurred during the period
 *        specified by pe_rm_timeout.
 *
 * IN resource_mgr
 * IN job_event - The address of the pointer to the job_event_t
 *        type. If an event is generated successfully by the resource
 *        manager, that event is saved at the location specified, and
 *        pe_rm_get_event returns 0 (or a nonzero value, if the event
 *        is not generated successfully). Based on the event type that is
 *        returned, the appropriate event of the type job_event_t can
 *        be accessed. After the event is processed, it should be
 *        freed by calling pe_rm_free_event.
 * OUT error_msg - The address of a character string at which the
 *        error message that is generated by pe_rm_get_event is
 *        stored. The memory for this error message is allocated by
 *        the malloc API call. After the error message is processed,
 *        the memory allocated should be freed by a calling free function.
 * IN rm_timeout - The integer value that defines a connection timeout
 *        value. This value is defined by the MP_RETRY environment
 *        variable. A value less than zero indicates there is no
 *        timeout. A value equal to zero means to immediately return
 *        with no wait or retry. A value greater than zero means to
 *        wait the specified amount of time (in seconds).
 * RET 0 - SUCCESS, nonzero on failure.
 */
extern int pe_rm_get_event(rmhandle_t resource_mgr, job_event_t **job_event,
			   int rm_timeout, char ** error_msg)
{
	return 0;
}

/* The pe_rm_get_job_info function is called to return job
 * information, after a job has been started. It can be called in
 * either batch or interactive mode. For interactive jobs, it should
 * be called when pe_rm_get_event returns with the JOB_STATE_EVENT
 * event type, indicating the JOB_STATE_RUNNING
 * state. pe_rm_get_job_info provides the job information data values,
 * as defined by the job_info_t structure. It returns with an error if
 * the job is not in a running state. For batch jobs, POE calls
 * pe_rm_get_job_info immediately because, in batch mode, POE is
 * started only after the job has been started. The pe_rm_get_job_info
 * function must be capable of being called multiple times from the
 * same process or a different process, and the same job data must be
 * returned each time. When called from a different process, the
 * environment of that process is guaranteed to be the same as the
 * environment of the process that originally called the function.
 *
 * IN resource_mgr
 * IN job_info - The address of the pointer to the job_info_t
 *        type. The job_info_t type contains the job information
 *        returned by the resource manager for the handle that is
 *        specified. The caller itself must free the data areas that
 *        are returned.
 * OUT error_msg - The address of a character string at which the
 *        error message that is generated by pe_rm_get_job_info is
 *        stored. The memory for this error message is allocated by the
 *        malloc API call. After the error message is processed, the memory
 *        allocated should be freed by a calling free function.
 * RET 0 - SUCCESS, nonzero on failure.
 */
extern int pe_rm_get_job_info(rmhandle_t resource_mgr, job_info_t **job_info,
			      char ** error_msg)
{
	return 0;
}

/* The handle to the resource manager is returned to the calling
 * function. The calling process needs to use the resource manager
 * handle in subsequent resource manager API calls.
 *
 * A version will be returned as output in the rmapi_version
 * parameter, after POE supplies it as input. The resource manager
 * returns the version value that is installed and running as output.
 *
 * A resource manager ID can be specified that defines a job that is
 * currently running, and for which POE is initializing the resource
 * manager. When the resource manager ID is null, a value for the
 * resource manager ID is included with the job information that is
 * returned by the pe_rm_get_job_info function. When pe_rm_init is
 * called more than once with a null resource manager ID value, it
 * returns the same ID value on the subsequent pe_rm_get_job_info
 * function call.
 *
 * The resource manager can be initialized in either
 * batch or interactive mode. The resource manager must export the
 * environment variable PE_RM_BATCH=yes when in batch mode.
 *
 * By default, the resource manager error messages and any debugging
 * messages that are generated by this function, or any subsequent
 * resource manager API calls, should be written to STDERR. Errors are
 * returned by way of the error message string parameter.
 *
 * When the resource manager is successfully instantiated and
 * initialized, it returns with a file descriptor for a listen socket,
 * which is used by the resource manager daemon to communicate with
 * the calling process. If a resource manager wants to send
 * information to the calling process, it builds an appropriate event
 * that corresponds to the information and sends that event over the
 * socket to the calling process. The calling process could monitor
 * the socket using the select API and read the event when it is ready.
 *
 * IN/OUT rmapi_version - The resource manager API version level. The
 *        value of RM_API_VERSION is defined in permapi.h. Initially,
 *        POE provides this as input, and the resource manager will
 *        return its version level as output.
 * OUT resource_mgr - Pointer to the rmhandle_t handle returned by the
 *        pe_rm_init function. This handle should be used by all other
 *        resource manager API calls.
 * IN/OUT rm_id - Pointer to a character string that defines a
 *        resource manager ID, for checkpoint and restart cases. This
 *        pointer can be set to NULL, which means there is no previous
 *        resource manager session or job running. When it is set to a
 *        value, the resource manager uses the specified ID for
 *        returning the proper job information to a subsequent
 *        pe_rm_get_job_info function call.
 * OUT error_msg - The address of a character string at which the
 *        error messages generated by this function are stored. The
 *        memory for this error message is allocated by the malloc API
 *        call. After the error message is processed, the memory
 *        allocated should be freed by a calling free function.
 * RET - Non-negative integer representing a valid file descriptor
 *        number for the socket that will be used by the resource
 *        manager to communicate with the calling process. - SUCCESS
 *        integer less than 0 - FAILURE
 */
extern int pe_rm_init(int *rmapi_version, rmhandle_t *resource_mgr, char *rm_id,
		      char** error_msg);
{
	return 0;
}

/* Used to inform the resource manager that a checkpoint is in
 * progress or has completed. POE calls pe_rm_send_event to provide
 * the resource manager with information about the checkpointed job.
 *
 * IN resource_mgr
 * IN job_event - The address of the pointer to the job_info_t type
 *        that indicates if a checkpoint is in progress (with a type
 *        of JOB_CKPT_IN_PROGRESS) or has completed (with a type of
 *        JOB_CKPT_COMPLETE).
 * OUT error_msg - The address of a character string at which the
 *        error message that is generated by pe_rm_send_event is
 *        stored. The memory for this error message is allocated by the
 *        malloc API call. After the error message is processed, the
 *        memory allocated should be freed by a calling free function.
 * RET 0 - SUCCESS, nonzero on failure.
 */
extern int pe_rm_send_event(rmhandle_t resource_mgr, job_event_t *job_event,
			    char ** error_msg)
{
	return 0;
}

/* This function is used to submit an interactive job to the resource
 * manager. The job request is either an object or a file (JCL format)
 * that contains information needed by a job to run by way of the
 * resource manager.
 *
 * IN resource_mgr
 * IN job_cmd - The job request (JCL format), either as an object or a file.
 * OUT error_msg - The address of a character string at which the
 *        error messages generated by this function are stored. The
 *        memory for this error message is allocated by the malloc API
 *        call. After the error message is processed, the memory
 *        allocated should be freed by a calling free function.
 * RET 0 - SUCCESS, nonzero on failure.
 */
int pe_rm_submit_job(rmhandle_t resource_mgr, job_command_t job_cmd,
		     char** error_msg)
{
	return 0;
}

