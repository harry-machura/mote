#ifndef MOTEC_ADDITIONS_H
#define MOTEC_ADDITIONS_H

#include "motec_core.h"

// Integrations-Hooks
void parse_if(P* p);
void parse_while(P* p);
void parse_for(P* p);
void parse_do_while(P* p);
void parse_switch(P* p);
void parse_break(P* p);
void parse_continue(P* p);
void parse_import(P* p);
void parse_assignment_or_call_stmt(P* p);

void implement_short_circuit_and(P* p);
void implement_short_circuit_or(P* p);

#endif
