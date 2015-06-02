/*
 *   splitstream_json.c
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

size_t SplitstreamJSONScanner(SplitstreamState* s, const char* buf, size_t len, size_t* start) {
    for(unsigned int i = 0; i < len; ++i) {
        switch(buf[i]) {
            case '{':
            case '[':
                if(s->state == State_Init) {
                    *start = i;
                    s->state = State_Document;
                }
                if(s->state != State_String)
                    ++s->depth;
                break;
            case '}':
            case ']':
                if(s->state == State_Document) {
                    if(--s->depth == s->startDepth) {
                        s->last = buf[i];
                        return i + 1;
                    }
                }
                break;
            case '"':
                if(s->state == State_String) {
                    if(s->last != '\\') {
                        s->state = State_Document;
                    }
                } else {
                    s->state = State_String;
                }
                break;
        }
        s->last = buf[i];
    }
    return 0;
}