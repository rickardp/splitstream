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

#include <splitstream_private.h>

size_t SplitstreamJSONScanner(SplitstreamState* s, const char* buf, size_t len, size_t* start) {
	int escapeCounter = s->counter[0];
    SplitstreamTokenizerState state = s->state;
    const char* end = buf + len, *cp = buf;
    
    #define LOOP_BEGIN \
	    for(; cp != end; ++cp) { \
    	    char c = *cp; \
	        switch(c) {
	        
	#define LOOP_END \
			} \
	        s->last = c; \
		} \
		goto h_End;
	
	#define TRANSITION(x) \
		state = State_##x; \
		++cp; \
		goto h_##x;
	
		switch(state) {
			case State_Init:
				goto h_Init;
			case State_Document:
				goto h_Document;
			case State_String:
				goto h_String;
			default:
				abort();
		}
		
	h_Init:
		LOOP_BEGIN
		case '[':
		case '{':
            *start = (cp - buf);
            ++s->depth;
            TRANSITION(Document)
			break;
        case '"':
        	TRANSITION(String);
        	break;
		LOOP_END
		
	h_Document:
		LOOP_BEGIN
		case '[':
		case '{':
		    if (s->depth == s->startDepth && s->startDepth > 0) {
				*start = (cp - buf);
			}
            ++s->depth;
            break;
		case ']':
		case '}':
            if(--s->depth == s->startDepth) {
				s->last = c;
				s->state = state;
				s->counter[0] = 0;
				return (cp - buf + 1);
			}
            break;
        case '"':
        	TRANSITION(String);
        	break;
		LOOP_END
		
	h_String:
		LOOP_BEGIN
			default:
				escapeCounter = 0;
				break;
            case '"':
                if(!(escapeCounter & 1)) {
					escapeCounter = 0;
                	TRANSITION(Document)
                }
				escapeCounter = 0;
				break;
            case '\\':
            	++escapeCounter;
            	break;
		LOOP_END
		
    h_End:
		s->state = state;
    	s->counter[0] = escapeCounter;
    return 0;
}
