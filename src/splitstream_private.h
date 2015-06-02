/*
 *   splitstream_private.h
 *   splitstream - Stream object splitter
 *
 *   Copyright © 2015 Rickard Lyrenius
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */

#ifndef __SPLITSTREAM_PRIVATE_H_INC
#define __SPLITSTREAM_PRIVATE_H_INC

typedef enum {
    State_Init,
    State_Document,
    State_ElementOrComment,
    State_CommentOrInstruction,
    State_BeginElement,
    State_EmptyElement,
    State_EndElement,
    State_Instruction,
    State_Comment,
    State_Cdata,

    State_String,

    State_Rescan
} SplitstreamTokenizerState;

#include "splitstream.h"

#endif /* __SPLITSTREAM_PRIVATE_H_INC */
