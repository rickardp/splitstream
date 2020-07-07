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

size_t SplitstreamUBJSONScanner(SplitstreamState* s, const char* buf, size_t len, size_t* start) {
	int remainingCounter = s->counter[0], value = s->counter[1];
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
			case State_LengthType:
				goto h_LengthType;
			case State_Length:
				goto h_Length;
			default:
				abort();
		}
		
	h_Init:
	h_Document:
		LOOP_BEGIN
		case '[':
		case '{':
			if(state == State_Init) {
            	*start = (cp - buf);
            }
            ++s->depth;
            TRANSITION(Document)
			break;
        case 'S': /* String */
        case 'H': /* High-precision number */
        	TRANSITION(LengthType);
        	break;
        case 'C': /* char */
        case 'i': /* int8 */
        case 'U': /* uint8 */
        	remainingCounter = 1;
        	TRANSITION(String);
        	break;
        case 'I': /* int16 */
        	remainingCounter = 2;
        	TRANSITION(String);
        	break;
        case 'l': /* int32 */
        case 'd': /* float */
        	remainingCounter = 4;
        	TRANSITION(String);
        	break;
        case 'L': /* int64 */
        case 'D': /* double */
        	remainingCounter = 8;
        	TRANSITION(String);
        	break;
		case ']':
		case '}':
            if(--s->depth == s->startDepth && state != State_Init) {
				s->last = c;
				s->state = state;
				s->counter[0] = s->counter[1] = 0;
				return (cp - buf + 1);
			}
            break;
		LOOP_END
		
	h_String:
		LOOP_BEGIN
			default:
				if(--remainingCounter <= 0) {
					remainingCounter = 0;
					TRANSITION(Document);
				}
            	break;
		LOOP_END
		
    			
	h_LengthType:
		LOOP_BEGIN
    		case 'i':
    		case 'U':
    			remainingCounter = 1;
    			value = 0;
    			TRANSITION(Length);
    			break;
    		case 'I':
    			remainingCounter = 2;
    			value = 0;
    			TRANSITION(Length);
    			break;
    		case 'l':
    			remainingCounter = 4;
    			value = 0;
    			TRANSITION(Length);
    			break;
    		default: /* We do not support 64-bit lengths */
	    		remainingCounter = 0;
    			TRANSITION(Document);
    			break;
		LOOP_END
		
	h_Length:
		LOOP_BEGIN
			default:
				value = ((int)(unsigned char)c) | (value << 8);
				if(--remainingCounter <= 0) {
					remainingCounter = value;
					value = 0;
					TRANSITION(String);
				}
				break;
		LOOP_END
		
    h_End:
		s->state = state;
    	s->counter[0] = remainingCounter;
    	s->counter[1] = value;
    return 0;
}
