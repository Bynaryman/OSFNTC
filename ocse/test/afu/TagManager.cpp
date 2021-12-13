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

#include "TagManager.h"

#include <stdlib.h>

std::set < uint32_t > TagManager::tags_in_use;
int TagManager::num_credits = 0;
int TagManager::max_credits = 255;
uint8_t TagManager::resp_credit = 0;
uint8_t TagManager::cmd_credit  = 0;
uint8_t TagManager::resp_data_credit = 0;
uint8_t TagManager::cmd_data_credit  = 0;

bool TagManager::request_tag (uint32_t * new_tag)
{

//    if (max_credits == 0)
//        warn_msg("TagManager: attempting to request tag when maximum available credit is 0. Did you forget to set room? ");

    // no more available credits
    if (num_credits == 0)
        return false;

    do {
        // randomly generate number between 0 - MAX_TAG_NUM
        *new_tag = (uint32_t) (rand () % (MAX_TAG_NUM + 1));
    } while (tags_in_use.find (*new_tag) != tags_in_use.end ());

    tags_in_use.insert (*new_tag);

//    debug_msg("TagManager::request_tag: insert new_tag = %d", *new_tag);

//    --num_credits;

    return true;
}

void
TagManager::release_tag (uint32_t tag)
{
//    debug_msg ("=====>TagManager::release_tag: %d", tag);
    release_tag (tag, 1);
}

void
TagManager::release_tag (uint32_t tag, int returned_credits)
{
    if (tags_in_use.find (tag) == tags_in_use.end ())
        error_msg ("TagManager: attempt to release tag not in use");

    tags_in_use.erase (tag);
    num_credits += returned_credits;

//    if (num_credits > max_credits)
//        error_msg
//        ("TagManager: more credits available than maximum allowed credits");
}

bool TagManager::is_in_use (uint32_t tag)
{
    for (std::set<uint32_t>::iterator it=tags_in_use.begin(); it != tags_in_use.end(); it++) {
	debug_msg ("TagManager::is_in_use: tags_in_use = %d", *it);
    }
    
//    debug_msg("TagManager::is_in_use: tag = %d ", tag);
    if (tags_in_use.find (tag) == tags_in_use.end ()) {
	debug_msg ("TagManager::is_in_use: tag = %d return false", tag);
        return false;
    }

    return true;
}

void
TagManager::reset ()
{
    tags_in_use.clear ();
    num_credits = max_credits;
}

void
TagManager::set_max_credits (int mc)
{
    max_credits = mc;
    num_credits = max_credits;
}

// reset credit
void
TagManager::reset_tlx_credit(uint8_t cmd_max_credit, uint8_t data_max_credit )
{
    //info_msg("TagManager: Initialize TLX cmd and data credits");
    resp_credit = cmd_max_credit;
    cmd_credit  = cmd_max_credit;
    resp_data_credit = data_max_credit;
    cmd_data_credit  = data_max_credit;
}

// request credit
bool
TagManager::request_tlx_credit(uint8_t type)
{
    switch(type) {
	case CMD_CREDIT:
	   if(cmd_credit > 0) {
		cmd_credit--;
		debug_msg("TagManager: Request: cmd credit = %d", cmd_credit);
	    }
	    else {
		error_msg("TagManager: No Command credit available");
		return false;
	    }
	    break;
	case RESP_CREDIT:
	    if(resp_credit > 0) {
		resp_credit--;
		debug_msg("TagManager: Request: resp credit = %d", resp_credit);
	    }
	    else {
		error_msg("TagManager: No Response credit available");
		return false;
	    }
	    break;
	case CMD_DATA_CREDIT:
	    if(cmd_data_credit > 0) {
		cmd_data_credit--;
		debug_msg("TagManager: Request: cmd data credit = %d", cmd_data_credit);
	    }
	    else {
		error_msg("TagManager: No Command Data credit available");
		return false;
	    }
	    break;
	case RESP_DATA_CREDIT:
	    if(resp_data_credit > 0) {
		resp_data_credit--;
		debug_msg("TagManager: Request: resp data credit = %d", resp_data_credit);
	    }
	    else {
		error_msg("TagManager: No Response Data credit available");
	   	return false;
	    }
	    break;
	default:
	    break;
    }
    return true;
}

// release credit
void
TagManager::release_tlx_credit(uint8_t type)
{
    switch(type) {
	case CMD_CREDIT:
	    cmd_credit++;
	    debug_msg("TagManager: Release: cmd_credit = %d", cmd_credit);
	    break;
	case RESP_CREDIT:
	    resp_credit++;
	    debug_msg("TagManager: Release: resp_credit = %d", resp_credit);
	    break;
	case CMD_DATA_CREDIT:
	    cmd_data_credit++;
	    debug_msg("TagManager: Release: cmd_data_credit = %d", cmd_data_credit);
	    break;
	case RESP_DATA_CREDIT:
	    resp_data_credit++;
	    debug_msg("TagManager: Release: resp_data_credit = %d", resp_data_credit);
	    break;
	default:
	    break;
    }
}

