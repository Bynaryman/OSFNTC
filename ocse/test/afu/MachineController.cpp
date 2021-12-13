/*
 * Copyright 2015,2017 International Business Machines
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "MachineController.h"
#include "Machine.h"

#include <stdlib.h>

MachineController::MachineController ():machines (NUM_MACHINES),
    tag_to_machine ()
{
    flushed_state = false;

    for (uint32_t i = 0; i < machines.size (); ++i)
        machines[i] = new Machine (0);
}

MachineController::MachineController (uint16_t ctx):machines (NUM_MACHINES),
    tag_to_machine ()
{
    flushed_state = false;

    for (uint32_t i = 0; i < machines.size (); ++i)
        machines[i] = new Machine (ctx);

}

bool MachineController::send_command (AFU_EVENT * afu_event, uint32_t cycle)
{
    bool  try_send = true;

    // allocate a tag
    uint32_t  tag;

    if (!TagManager::request_tag (&tag)) {
        debug_msg ("MachineController::send_command: no more tags available");
        try_send = false;
    }

    // attempt to send a command with the allocated tag
    for (uint32_t i = 0; i < machines.size (); ++i) {
        if (try_send && machines[i]->is_enabled ()
                && machines[i]->attempt_new_command (afu_event, tag,
                        flushed_state, (uint16_t) (cycle & 0x7FFF)))
        {
            debug_msg
            ("MachineController::send_command: machine id %d sent new command", i);
            try_send = false;
            tag_to_machine[tag] = machines[i];
	    resend_machine = machines[i];
  	    resend_tag = tag;
	    memcpy(&resend_afu_event, afu_event, sizeof(resend_afu_event));
	    debug_msg("MachineController::send_command tag = 0x%x machine = 0x%x", tag, machines[i]);
        }

        // regardless if a command is sent, notify machine to advanced one cycle in delaying phase
        machines[i]->advance_cycle ();
    }

    // tag was not used by any machine if try_send is still true therefore return it
    if (try_send)
        TagManager::release_tag (tag);

    return !try_send;
}

bool
MachineController::resend_command(AFU_EVENT *afu_event, uint32_t cycle)
{
    bool try_send = true;

    debug_msg("MachineController::resend_command");
    resend_machine->attempt_resend_command(&resend_afu_event, resend_tag, flushed_state, 
			(uint16_t)(cycle & 0x7FFF));
    return try_send;     
}
 
void
MachineController::set_machine_enable_bit(uint8_t position)
{
    uint32_t i;
    for(i=0; i<machines.size(); i++) {
	
    }
}
void
MachineController::process_response (AFU_EVENT * afu_event, uint32_t cycle)
{
}

void
MachineController::process_buffer_write (AFU_EVENT * afu_event)
{
}

void
MachineController::process_buffer_read (AFU_EVENT * afu_event)
{
}

void
MachineController::change_machine_config (uint16_t index, uint16_t machine_number, uint64_t data)
{
    machines[machine_number]->change_machine_config(index, data);

    if (machine_number >= NUM_MACHINES) {
        warn_msg
        ("MachineController::change_machine_config: word address exceeded machine configuration space");
        return;
    }
}

void
MachineController::get_machine_config (uint16_t context)
{
   
}

void
MachineController::reset ()
{
    flushed_state = false;
    for (uint32_t i = 0; i < machines.size (); ++i)
        machines[i]->reset ();
}

bool MachineController::is_enabled () const
{
    for (uint32_t i = 0; i < machines.size (); ++i)
    {
        if (machines[i]->is_enabled ()) {
            return true;
        }
    }

    return
        false;
}

bool MachineController::all_machines_completed () const
{
    for (uint32_t i = 0; i < machines.size (); ++i)
    {
        if (!machines[i]->is_completed ()) {
            return false;
        }
    }

    return
        true;
}

void
MachineController::disable_all_machines ()
{
    for (uint32_t i = 0; i < machines.size (); ++i)
        machines[i]->disable ();
}

bool MachineController::has_tag (uint32_t tag) const
{
    if (tag_to_machine.find (tag) != tag_to_machine.end ())
        return true;

    return false;
}

MachineController::~
MachineController ()
{
    for (uint32_t i = 0; i < machines.size (); ++i)
        if (machines[i])
            delete machines[i];

}


