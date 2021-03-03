/***************************************************************************
Copyright 2021 Alan Crispin crispinalan@gmail.com                     

Redistribution and use in source and binary forms, with or without 
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, 
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, 
this list of conditions and the following disclaimer in the documentation 
and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" 
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE 
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER 
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, 
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE 
USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
***************************************************************************/


#ifndef DBMANAGER_H
#define DBMANAGER_H

#include "event.h"


void db_create_events_table();
int db_insert_event(Event *event);
void db_insert_event_struct(Event event);
void db_update_event(Event *event);

void db_get_event(int index, Event *event);

int db_get_number_of_rows_all();
int db_get_number_of_rows_month_year(int month, int year);
int db_get_number_of_rows_year_month_day(int year, int month, int day);

int db_get_number_of_rows_less_than(int year, int month, int day);


void db_get_all_events(Event *event_buff, int count);
void db_get_all_events_year_month(Event *event_buff, int year, int month, int count);
void db_get_all_events_year_month_day(Event *event_buff, int year, int month, int day, int count);

void db_delete_row(int id);
void db_delete_all();
void db_reset_sequence();



#endif // DBMANAGER_H
