/*
 *   splitstream_xml.c
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

#include "splitstream_private.h"

const int COUNTER_DASH = 0;
const int COUNTER_CLOSING_BRACKET = 1;

size_t SplitstreamXMLScanner(SplitstreamState* s, const char* buf, size_t len, size_t* start) {
    int dashCounter = s->counter[COUNTER_DASH], bracketCounter = s->counter[COUNTER_CLOSING_BRACKET];
    SplitstreamTokenizerState state = s->state;
    const char* end = buf + len, *cp;
    for(cp = buf; cp != end; ++cp) {
        char c = *cp;
        switch(c) {
            default:
                if(state == State_ElementOrComment) {
                    state = State_BeginElement;
                } else if(state == State_CommentOrInstruction) {
                    state = State_Instruction;
                }
                dashCounter = bracketCounter = 0;
                break;
            case '<':
                if(state == State_Init || state == State_Document) {
                    if(state == State_Init || (s->depth == s->startDepth && s->startDepth > 0)) {
                        *start = (cp - buf);
                    }
                    state = State_ElementOrComment;
                }
                dashCounter = bracketCounter = 0;
                break;
            case '>':
                if(state == State_BeginElement || state == State_ElementOrComment) {
                    state = State_Document;
                    if(s->last != '/') ++s->depth;
                    else if(s->depth == s->startDepth) {
                        s->last = c;
                        s->state = state;
                        s->counter[COUNTER_DASH] = s->counter[COUNTER_CLOSING_BRACKET] = 0;
                        return (cp - buf + 1);
                    }
                } else if(state == State_EndElement) {
                    if(--s->depth == s->startDepth) {
                        s->last = c;
                        s->state = state;
                        s->counter[COUNTER_DASH] = s->counter[COUNTER_CLOSING_BRACKET] = 0;
                        return (cp - buf + 1);
                    } else {
                        state = State_Document;
                    }
                } else if((state == State_Comment && dashCounter >= 2) || state == State_Instruction) {
                    state = State_Document;
                } else if(state == State_Cdata && bracketCounter >= 2) {
                    state = State_Document;
                }
                dashCounter = bracketCounter = 0;
                break;
            case '/':
                if(state == State_ElementOrComment) {
                    state = State_EndElement;
                }
                dashCounter = bracketCounter = 0;
                break;
            case '?':
                if(state == State_ElementOrComment) {
                    state = State_Instruction;
                }
                dashCounter = bracketCounter = 0;
                break;
            case '!':
                if(state == State_ElementOrComment) {
                    state = State_CommentOrInstruction;
                }
                dashCounter = bracketCounter = 0;
                break;
            case '-':
                if(state == State_CommentOrInstruction && dashCounter > 0) {
                    state = State_Comment;
                }
                bracketCounter = 0;
                ++dashCounter;
                break;
            case '[':
                if(state == State_CommentOrInstruction) {
                    state = State_Cdata;
                }
                dashCounter = bracketCounter = 0;
                break;
            case ']':
                dashCounter = 0;
                ++bracketCounter;
                break;
        }
        s->last = c;
    }
    s->state = state;
    s->counter[COUNTER_DASH] = dashCounter;
    s->counter[COUNTER_CLOSING_BRACKET] = bracketCounter;
    return 0;
}
