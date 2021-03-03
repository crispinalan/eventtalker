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

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "dbmanager.h"
#include "event.h"


// Definitions
#define CONFIG_DIRNAME "etalker"
#define CONFIG_FILENAME "etalker"

//Static variables (configuration variables)
static char * m_config_file = NULL;

static GMutex lock;

//static int m_speak_flag=1;

static int m_speaking =0;
static int m_ahead=3;
static int m_speed = 175; //espeak words per minute
//static int  m_gap = 10 // 10 ms gap between words

//Prototypes
static void reload_events(GtkWidget *widget, int year, int month, int day);
static void etalker_new_event(GtkWidget *widget, guint year, guint month, guint day);
static void etalker_update_event(GtkWidget *widget, gpointer data);
gchar * scan_events_for_upcoming_events();
static gpointer thread_speak_func(gpointer user_data);
static void add_to_list(GtkWidget *list, const gint id, const gchar* title);
void remove_item(GtkWidget *list, gpointer selection);


static void config_load_default()
{	
	if(!m_speaking) m_speaking=1;
	if(!m_speed) m_speed=175;
	if(!m_ahead) m_ahead=3;
}

static void config_read()
{
	// Clean up previously loaded configuration values	
	m_speaking = 1;
	m_speed=175;
	m_ahead=3;
	
	// Load keys from keyfile
	GKeyFile * kf = g_key_file_new();
	g_key_file_load_from_file(kf, m_config_file, G_KEY_FILE_NONE, NULL);
		
	m_speaking = g_key_file_get_integer(kf, "tikk_settings", "speaking", NULL);
	m_speed = g_key_file_get_integer(kf, "tikk_settings", "speed", NULL);
	m_ahead = g_key_file_get_integer(kf, "tikk_settings", "ahead", NULL);
	
	g_key_file_free(kf);	
	//config_load_default();	
}

void config_write()
{
	
	GKeyFile * kf = g_key_file_new();	
	//(key-file, group, key, value)
	g_key_file_set_integer(kf, "tikk_settings", "speaking", m_speaking);
	g_key_file_set_integer(kf, "tikk_settings", "speed", m_speed);
	g_key_file_set_integer(kf, "tikk_settings", "ahead", m_ahead);
	
	gsize length;
	gchar * data = g_key_file_to_data(kf, &length, NULL);
	g_file_set_contents(m_config_file, data, -1, NULL);
	g_free(data);
	g_key_file_free(kf);
	
}

void config_initialize() {
	
	gchar * config_dir = g_build_filename(g_get_user_config_dir(), CONFIG_DIRNAME, NULL);
	m_config_file = g_build_filename(config_dir, CONFIG_FILENAME, NULL);

	// Make sure config directory exists
	if(!g_file_test(config_dir, G_FILE_TEST_IS_DIR))
		g_mkdir(config_dir, 0777);

	// If a config file doesn't exist, create one with defaults otherwise
	// read the existing one
	if(!g_file_test(m_config_file, G_FILE_TEST_EXISTS))
	{
		config_load_default();
		config_write();
	}
	else
	{
		config_read();
	}

	g_free(config_dir);
	
}
static void import_callbk(GSimpleAction * action,
							G_GNUC_UNUSED GVariant      *parameter,
							              gpointer       user_data)
{
	//g_print("The action [%s] was activated\n", g_action_get_name(G_ACTION(action)));							  
	//g_print("import callback ...\n");
	GtkCalendar *calendar =user_data;
	
	gchar *file_name = NULL;        // Name of file to open from dialog box
    gchar *file_contents = NULL;    // For reading contents of file
    gboolean file_success = FALSE;  // File read status
    GtkWidget *dialog; 
    GtkFileFilter *filter1, *filter2;
    GtkTextIter start, end;
    GtkTextBuffer *textbuffer;
    textbuffer = gtk_text_buffer_new (NULL);
    gchar *text; 
     
    
    
     dialog = gtk_file_chooser_dialog_new ("Open",
                      NULL,
                      GTK_FILE_CHOOSER_ACTION_OPEN,
                      "_Cancel", GTK_RESPONSE_CANCEL,                      
                      "_Open", GTK_RESPONSE_ACCEPT,                      
                      NULL);
                      
	  filter1 = gtk_file_filter_new();
	  filter2 = gtk_file_filter_new();    
	  gtk_file_filter_add_pattern(filter1, "*.csv");
	  gtk_file_filter_set_name(filter1, "Comma Separated Values (*.csv)");
	  gtk_file_filter_add_pattern(filter2, "*.*");
	  gtk_file_filter_set_name (filter2, "All Files (*.*)");
	  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter1);
	  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter2);   
	  
	  
	  gtk_widget_show_all(dialog);
	  
	  gint resp = gtk_dialog_run(GTK_DIALOG(dialog));
	  
    if (resp == GTK_RESPONSE_ACCEPT){
		
		//g_print("Open pressed\n");
		file_name = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		
		if (file_name != NULL) {
            
            //g_file_get_contents(filename, &contents, &length, &error)
            // Copy the contents of the file to dynamically allocated memory
            file_success = g_file_get_contents(file_name, &file_contents, NULL, NULL);
            if (file_success) {
                // Put the contents of the file into the GtkTextBuffer
                gtk_text_buffer_set_text(textbuffer, file_contents, -1);
               
				gtk_text_buffer_get_bounds(textbuffer, &start, &end);
				text = gtk_text_buffer_get_text(textbuffer, &start, &end, FALSE);  
				//g_print("file text\n%s\n",text);
				gint line_count = gtk_text_buffer_get_line_count(textbuffer)-1;  //-1
			    //g_print("Line count = %d\n",line_count); 
			   
			   //Create event array with line count
			    Event event_array[line_count]; 
				 				
				//g_print("spliting text lines using newline token\n");
				gchar **lines = g_strsplit (text, "\n", -1);
				
				for (int i = 0; lines[i] != NULL; i++) {
					//g_print("%s\n",lines[i]);
					if (lines[i][0] == '\0') continue; //ignore blank lines
					//g_print("line[%d] = %s\n",i,lines[i]);
					
					//create an event from line and store
					//Event e;   
			   Event e; 
			   gchar** items = g_strsplit (lines[i],",",-1);
			   
			   gint j=0;			   
			   while(items[j] != NULL)
			   {
			   //g_print("%s\n",items[j]);			   
			   if (j==0) {
			   //g_print("title = %s\n",items[0]);
			   strcpy(e.title,items[0]);
			   }
			   else if(j==1) {
			   // g_print("priority = %s\n",items[1]); //need to convert to int			   
			   gint priority_num = -1;
			   priority_num = atoi(items[1]);
			   //g_print("priority as int number = %d\n",priority_num);
			   e.priority=priority_num;			   
			   }
			   else if(j==2) {
			   // g_print("description= %s\n",items[2]);
			   strcpy(e.description,items[2]);
			   }
			   else if(j==3) {
			   // g_print("year= %s\n",items[3]);
			   gint year = -1;
			   year = atoi(items[3]);
			   e.year=year;
			   //g_print("year = %d\n",year);
			   				   
			   }
			   else if(j==4) {
			   // g_print("month= %s\n",items[4]);
			   gint month = -1;
			   month = atoi(items[4]);
			   e.month=month;
			   //g_print("month = %d\n",month);			 				   
			   }
			   else if(j==5) {				  
			   gint day = -1;
			   day= atoi(items[5]);				  
			   e.day=day;				   
			   }
			   else if(j==6) {				  
			   gint startHour = -1;
			   startHour= atoi(items[6]);				  
			   e.startHour=startHour;				   
			   }
			   else if(j==7) {				  
			   gint startMin = -1;
			   startMin= atoi(items[7]);				  
			   e.startMin=startMin;				   
			   }
			   else if(j==8) {				  
			   gint endHour = -1;
			   endHour= atoi(items[8]);				  
			   e.endHour=endHour;				   
			   }
			   else if(j==9) {				  
			   gint endMin = -1;
			   endMin= atoi(items[9]);
			   e.endMin= endMin;				   
			   }
			   else if(j==10) {				  
			   gint isAllday= -1;
			   isAllday= atoi(items[10]);				  
			   e.isAllday=isAllday;				   
			   }
			   j++;		
			   } //while j
			   //insert event e into db
			   event_array[i]=e;					
			} //for lines[i]	
			          
            Event evt;	
			for(int k=0; k<line_count; k++) {		
			evt=event_array[k];			
			db_insert_event_struct(evt);
			} //for k   
		   g_free(file_contents);	
	    }//if success
	   g_free(file_name); 
	}//file name not null
	} //response Accept
	else {
		//g_print("Cancel pressed\n");
		gtk_widget_destroy(GTK_WIDGET(dialog));
		return;
	}
	

   guint cday, cmonth, cyear;
   gtk_calendar_get_date(GTK_CALENDAR (calendar), &cyear, &cmonth, &cday);
   //g_print("Calendar date  =%d-%d-%d\n", cday,cmonth,cyear);   
   gtk_calendar_clear_marks(GTK_CALENDAR (calendar));  
   gint row_number = db_get_number_of_rows_month_year(cmonth+1, cyear); 	
   if (row_number ==0) {		
		//g_print("No events for month\n");
		return;
	}
	else {	 
	 //g_print("Fetching month changed events\n");	
	 Event event_array[row_number];		 
	 db_get_all_events_year_month(event_array, cyear, cmonth+1, row_number);
	 //mark up calendar
	 Event evtmarkup;	
	for(int i=0; i<row_number; i++) {		
		evtmarkup=event_array[i];		//
		guint dday = evtmarkup.day;		
		gtk_calendar_mark_day(calendar, dday);
	} 	 
   }
      
   gtk_widget_destroy(GTK_WIDGET(dialog));  

}


static void export_callbk(GSimpleAction * action,
							G_GNUC_UNUSED GVariant      *parameter,
							              gpointer       user_data)
{
	//g_print("The action [%s] was activated\n", g_action_get_name(G_ACTION(action)));							  
	//g_print("export callback ...\n");
	
	GtkWidget *dialog; 
	GtkFileFilter *filter1, *filter2;   
    gchar *filename;
    gchar *csv_str;
    csv_str="";
        
    gint row_count_all = db_get_number_of_rows_all();
    
   if (row_count_all ==0) {		
		//g_print("No events to export\n");
		return;
	}
	
	else {			
	 Event event_array[row_count_all];		 
	 db_get_all_events(event_array,row_count_all);
	 
	 Event evt;	
	 for(int i=0; i<row_count_all; i++) {		
		evt=event_array[i];	
		
	    gchar *priority_str = g_strdup_printf("%i", evt.priority);	   
	    gchar *year_str = g_strdup_printf("%i", evt.year);
		gchar *month_str = g_strdup_printf("%i", evt.month);
		gchar *day_str = g_strdup_printf("%i", evt.day);
		gchar *start_hour_str = g_strdup_printf("%i", evt.startHour);
		gchar *start_min_str = g_strdup_printf("%i", evt.startMin);
		gchar *end_hour_str = g_strdup_printf("%i", evt.endHour);
		gchar *end_min_str = g_strdup_printf("%i", evt.endMin);
		gchar *is_allday_str = g_strdup_printf("%i", evt.isAllday);
	    
		csv_str= g_strconcat(csv_str, 
		evt.title, ",", 
		priority_str,",",
		evt.description, ",",
		year_str,",",
		month_str,",",
		day_str,",",
		start_hour_str,",",
		start_min_str,",",
		end_hour_str,",",
		end_min_str,",",
		is_allday_str,"\n", //terminate with new line	
		NULL);
		
	  }//for i events	
	
     }
    
    dialog = gtk_file_chooser_dialog_new ("Save",
									NULL,
									GTK_FILE_CHOOSER_ACTION_SAVE,                      
									"_Cancel", GTK_RESPONSE_CANCEL,
									"_Save", GTK_RESPONSE_ACCEPT, 
									NULL);
									
    // gtk_window_set_default_size (GTK_WINDOW (dialog), 220, 200);   
    
    filter1 = gtk_file_filter_new();
    filter2 = gtk_file_filter_new();
    
    gtk_file_filter_add_pattern(filter1, "*.csv");
    gtk_file_filter_set_name(filter1, "Comma Separated Values (*.csv)");
   
    
    gtk_file_filter_add_pattern(filter2, "*.*");
    gtk_file_filter_set_name (filter2, "All Files (*.*)");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter1);
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter2);    
    
    // default file name
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "filename.csv");
    gtk_widget_show_all(dialog);
    
    gint resp = gtk_dialog_run(GTK_DIALOG(dialog));
    
    if (resp == GTK_RESPONSE_ACCEPT){		
   
    filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
   
	
    //g_print("filename = %s\n", filename);	
   // g_print("csv data = %s\n",csv_str);	
    g_file_set_contents (filename, csv_str, -1,NULL);
    
    //g_free (text); //core dumped invalid pointer
    g_free (filename);
    
	}
    else{
    //g_print("You pressed the cancel\n");
    //gtk_widget_destroy(GTK_WIDGET(dialog));
	}
    gtk_widget_destroy(GTK_WIDGET(dialog));
	
}


//---------------------------------------------------------------------
// exit callback
//---------------------------------------------------------------------

static void exit_callbk(GSimpleAction * action,
							G_GNUC_UNUSED GVariant      *parameter,
							              gpointer       user_data)
{
	//g_print("The action [%s] was activated\n", g_action_get_name(G_ACTION(action)));							  
	//g_print("exit callback ...");
	
	g_application_quit(G_APPLICATION(user_data));
	
}


static void options_close_callbk(GtkDialog *dialog,
					     G_GNUC_UNUSED gpointer   user_data)  
{
	if( !GTK_IS_DIALOG(dialog)) {	  
	//g_print("Not a dialog -returning\n");
	return;
	}	
	//g_print("ESCAPE key was pressed -closing dialog\n");
} 

static void etalker_options_dialog(GtkWidget *widget) {
	
	GtkWidget *window = (GtkWidget *) widget;
   
   if( !GTK_IS_WINDOW(window)) {	  
	  //g_print("Widget is not a GtkWindow\n");
	  return;
  }
  
  GtkWidget *dialog;    
  GtkWidget *container;	
  
  GtkWidget *checkbutton_speaking;
  
  GtkWidget *label_speed;	
  GtkWidget *spin_button_speed;  
  GtkWidget *box_speed;
  
  //reading days ahead	
  GtkWidget *label_ahead;
  GtkWidget *combo_box_ahead;
  GtkWidget *box_ahead;
  
  
  gint response;
  
  //Create dialog
    dialog= gtk_dialog_new(); 
    gtk_window_set_title (GTK_WINDOW (dialog), "Options"); 
    g_signal_connect_swapped(dialog,"close",G_CALLBACK(options_close_callbk),dialog);//escape close
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(window));  
   
    gtk_widget_set_size_request(dialog, 300,200);
    gtk_window_set_modal(GTK_WINDOW(dialog),TRUE);  
    gtk_dialog_add_button(GTK_DIALOG(dialog),"Ok",1);  
    gtk_dialog_add_button(GTK_DIALOG(dialog),"Cancel",2);
    
        
    checkbutton_speaking = gtk_check_button_new_with_label ("Speaking");
    //gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton_speaking), TRUE);      
     gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkbutton_speaking), m_speaking);   //reminder_state=0;
    //g_signal_connect (GTK_TOGGLE_BUTTON (checkbutton_speaking), "toggled", G_CALLBACK (checkbutton_speaking_toggled_callbk), NULL);
	
    label_speed =gtk_label_new("Speed (words per minute)");	
	spin_button_speed = gtk_spin_button_new_with_range(100.0,200.0,1.0); //23 hours	
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_button_speed),0);
	//gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_button_speed),1.0,200.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button_speed),TRUE);
	//gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_speed),175.0);	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_speed),m_speed);	
	//g_signal_connect(spin_button_speed, "output", G_CALLBACK (on_output_speed), NULL);
	box_speed=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_speed),label_speed,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_speed),spin_button_speed,TRUE, TRUE, 5);
	
	
	label_ahead =gtk_label_new("Speak Events: ");  
	/* Create the combo box and append your string values to it. */
    combo_box_ahead = gtk_combo_box_text_new ();
    const char *ahead[] = {"Today Only", 
	"Next Day", 
	"Next 2 Days", 
	"Next 3 Days", 
	"Next 4 Days", 
	"Next 5 Days"};
	
	
	/* G_N_ELEMENTS is a macro which determines the number of elements in an array.*/ 
	for (int i = 0; i < G_N_ELEMENTS (ahead); i++){
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box_ahead), ahead[i]);
	}
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box_ahead),m_ahead);	
	//g_signal_connect (combo_box_ahead,"changed", G_CALLBACK (callbk_combo_box_ahead_on_changed), NULL);
	box_ahead=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_ahead),label_ahead,FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box_ahead),combo_box_ahead,TRUE, TRUE, 0);
	

	
	
	
	//--------------------------------------------------------------
	// Dialog PACKING
	//--------------------------------------------------------------  
	container = gtk_dialog_get_content_area (GTK_DIALOG(dialog));	
	gtk_container_add(GTK_CONTAINER(container),checkbutton_speaking);
	gtk_container_add(GTK_CONTAINER(container),box_speed);
	gtk_container_add(GTK_CONTAINER(container),box_ahead);			
    gtk_widget_show_all(GTK_WIDGET(dialog)); 
    
     response = gtk_dialog_run(GTK_DIALOG(dialog));  
   
  
  switch (response)
  {
  case 1:
  //g_print("The Ok button clicked\n");
  
  m_speaking =gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(checkbutton_speaking));
  m_speed =gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_button_speed));
  
   
 // g_print("Options Dialog: m_speaking set to %d\n", m_speaking);
 // g_print("Options Dialog: m_speed set to %d\n", m_speed);
  
  gint active_id = gtk_combo_box_get_active (GTK_COMBO_BOX(combo_box_ahead));
  
	  switch (active_id)
	  {
		  case 0:
		  m_ahead=0; //today only
		  break;
		  case 1:
		  m_ahead=1; //today and next day
		  break;
		  case 2:
		  m_ahead=2; //next 2 days
		  break;
		  case 3:
		  m_ahead=3; //next 3 days
		  break;
		  case 4:
		  m_ahead=4; //next 4 days
		  break;
		  case 5:
		  m_ahead=5; //next 5 days
		  break;		  
		  default:
		  m_ahead=0; //today only
		  break; 
	  }
  
   
  config_write();
  
  gtk_widget_destroy(GTK_WIDGET(dialog));	 
  break;	 
  
   case 2:
  //g_print("The Cancel button was pressed\n");  
  gtk_widget_destroy(GTK_WIDGET(dialog));	 
  break;	  
  default:
  gtk_widget_destroy(GTK_WIDGET(dialog));
  break;
} 
	
}


static void callbk_gotoday(GSimpleAction *action, G_GNUC_UNUSED  GVariant  *parameter,	gpointer window)
{
	
//g_print("Go to today callbk\n");

GtkCalendar *calendar =g_object_get_data(G_OBJECT(window), "calendar-key");
GtkWidget *label_date =g_object_get_data(G_OBJECT(window), "label-date-key");

GDateTime *dt;  
gchar *dt_format;  
dt = g_date_time_new_now_local();

gint year = g_date_time_get_year(dt);
gint month =g_date_time_get_month(dt);    
gint day = g_date_time_get_day_of_month(dt); 
//g_print("current year = %d\n", year);
//g_print("current month = %d\n", month);
//g_print("current day = %d\n", day);
dt_format = g_date_time_format(dt, "%a %e %b %Y"); 
gtk_label_set_xalign(GTK_LABEL(label_date),0.5);
gtk_label_set_text(GTK_LABEL(label_date), dt_format); 

gtk_calendar_select_month (calendar, month-1, year);
gtk_calendar_select_day (calendar, day);
reload_events(window, year, month,day);

//gtk_calendar_set_date(GTK_CALENDAR (calendar), &year, &month, &day);

g_date_time_unref (dt);

}

static void callbk_upcoming_events(GSimpleAction *action,
							G_GNUC_UNUSED  GVariant      *parameter,
							  gpointer       user_data){
	
	//g_print("Called scan action callbk...\n");
      
   //GtkWidget *message_dialog;
   GThread *thread_speak_upcoming; 
	gchar* message_speak =scan_events_for_upcoming_events();  
		
	if(m_speaking) {	
	g_mutex_lock (&lock);
    thread_speak_upcoming = g_thread_new(NULL, thread_speak_func, message_speak);   
	}	
	g_thread_unref (thread_speak_upcoming);
	
    
}
//--------------------------------------------------------------------
// Options callback
//--------------------------------------------------------------------
static void options_callbk(GSimpleAction *action,
							G_GNUC_UNUSED  GVariant      *parameter,
							  gpointer       user_data){
	
	GtkWidget *window = (GtkWidget *) user_data;
   
   if( !GTK_IS_WINDOW(window)) {	  
	  //g_print("user_data is not a GtkWindow\n");
	  return;
  }
	
		
	etalker_options_dialog(window);
	
		
}



//---------------------------------------------------------------------
// spin button show leading zeros
//---------------------------------------------------------------------
static gboolean on_output (GtkSpinButton *spin, gpointer data)
{
   GtkAdjustment *adjustment;
   gchar *text;
   int value;
   adjustment = gtk_spin_button_get_adjustment (spin);
   value = (int)gtk_adjustment_get_value (adjustment);
   text = g_strdup_printf ("%02d", value);
   gtk_entry_set_text (GTK_ENTRY (spin), text);
   g_free (text);
   return TRUE;
}


//--------------------------------------------------------------------
// About
//---------------------------------------------------------------------

static void callbk_about_close (GtkDialog *dialog, gint response_id, gpointer user_data)
{
  /* This will cause the dialog to be destroyed */
  gtk_widget_destroy (GTK_WIDGET (dialog));
}


static void callbk_about(GtkWindow *window){	
	
	if(m_speaking>=1) {	
	GThread *thread;
	gchar *about_str ="Event Talker is a talking events assistant.\
	 I am the first Gtk version and so do not expect too much from me";
    //g_print("Event Talker about speaking s %s\n", about_str);
	g_mutex_lock (&lock);
	thread = g_thread_new(NULL, thread_speak_func, about_str);
	}
		
	//g_print("The about callback activated...\n");	
	const gchar *authors[] = {"Alan Crispin", NULL};		
	GtkWidget *about_dialog;
	about_dialog = gtk_about_dialog_new();	
	g_signal_connect (GTK_DIALOG (about_dialog), "response", G_CALLBACK (callbk_about_close), NULL);
	
	gtk_window_set_transient_for(GTK_WINDOW(about_dialog),GTK_WINDOW(window));
	gtk_widget_set_size_request(about_dialog, 200,200);
    gtk_window_set_modal(GTK_WINDOW(about_dialog),TRUE);	
	gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(about_dialog), "Event Talker");
	gtk_about_dialog_set_version (GTK_ABOUT_DIALOG(about_dialog), "0.2.0");
	gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(about_dialog),"Copyright © 2021");
	gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(about_dialog),"Talking Events Assistant"); 	
	//gtk_about_dialog_set_wrap_license(GTK_ABOUT_DIALOG(about_dialog),TRUE);	
	gtk_about_dialog_set_license_type (GTK_ABOUT_DIALOG(about_dialog), GTK_LICENSE_BSD);
	gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(about_dialog),"https://gitlab.com/crispinalan/eventtalker"); 
	gtk_about_dialog_set_website_label(GTK_ABOUT_DIALOG(about_dialog),"Event Talker Website");
	gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(about_dialog), authors);
	//gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(about_dialog), NULL);	
	gtk_about_dialog_set_logo_icon_name(GTK_ABOUT_DIALOG(about_dialog), "x-office-calendar");
	gtk_widget_show(about_dialog);	
		
}

//---------------------------------------------------------------------
// List view  //G_GNUC_UNUSED
//---------------------------------------------------------------------

//-------------------------------------------------------------------
// List row activated (double click)
//-------------------------------------------------------------------

static void list_row_activated_callbk(GtkTreeView  *tree_view,
										 GtkTreePath       *path,
										 GtkTreeViewColumn *column,
										 gpointer           user_window)
										 
{
   
   //g_print("List row activated...\n");	
   gint id =0;   
   gchar *title;
  
   GtkWindow *window = user_window; //window   
   GtkTreeIter iter; //row iterator (internal structure)
   GtkTreeModel *model = gtk_tree_view_get_model(tree_view);

   if (gtk_tree_model_get_iter(model, &iter, path)) {      
   //Get selected data   
   //gtk_tree_model_get (GTK_LIST_STORE(model), &iter, 0, &id, 1, &title, -1);      
   gtk_tree_model_get (GTK_TREE_MODEL(model), &iter, 0, &id, 1, &title, -1);
   //g_print("id = %d\n",id);      
   //g_print("title =%s\n",title);
   Event *event =g_malloc(sizeof(Event));    
   db_get_event(id, event);    
   etalker_update_event(GTK_WIDGET(window), event); 
   }
}

//--------------------------------------------------------------------
// List view -inialise - add -remove -remove all
//--------------------------------------------------------------------

static void init_list(GtkWidget *list)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *id, *title; 
    GtkListStore *store;

    id = gtk_tree_view_column_new_with_attributes("id", renderer, "text", 0, NULL);      
    title = gtk_tree_view_column_new_with_attributes("title", renderer,"text",1, NULL);
 
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list),FALSE);

    gtk_tree_view_append_column(GTK_TREE_VIEW(list), id);
    gtk_tree_view_column_set_visible (id,FALSE); 
       
    gtk_tree_view_append_column(GTK_TREE_VIEW(list), title);
    gtk_tree_view_column_set_visible (title,TRUE); 
   
    store = gtk_list_store_new(2, G_TYPE_INT, G_TYPE_STRING);
    gtk_tree_view_set_model(GTK_TREE_VIEW(list), GTK_TREE_MODEL(store));
    g_object_unref(store);
}
          

//static void add_to_list(GtkWidget *list, const gchar *id, const gchar *year, const gchar *month, const gchar *day, const gchar* title)
static void add_to_list(GtkWidget *list, const gint id, const gchar* title)
{
    GtkListStore *store;
    GtkTreeIter iter;

    store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));

    gtk_list_store_append(store, &iter);
    gtk_list_store_set(store, &iter, 0, id, -1); 
    gtk_list_store_set(store, &iter, 1, title, -1);
}

static void update_list(GtkWidget *list, const gint id, const gchar* title, gpointer selection){
	
  GtkListStore *store;
  GtkTreeModel *model;
  GtkTreeIter  iter;

  store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));

  if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) {
      return;
  }

  if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), 
         &model, &iter)) {
    gtk_list_store_set(store, &iter, 0, id, -1);  
    gtk_list_store_set(store, &iter, 1, title, -1);
  }
	

}

void remove_all(GtkWidget *list, gpointer selection) {
    
  GtkListStore *store;
  GtkTreeModel *model;
  GtkTreeIter  iter;

  store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));

  if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) {
      return;
  }
  
  gtk_list_store_clear(store);
}

void remove_item(GtkWidget *list, gpointer selection) {
    
  GtkListStore *store;
  GtkTreeModel *model;
  GtkTreeIter  iter;

  store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(list)));
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(list));

  if (gtk_tree_model_get_iter_first(model, &iter) == FALSE) {
      return;
  }

  if (gtk_tree_selection_get_selected(GTK_TREE_SELECTION(selection), 
         &model, &iter)) {
    gtk_list_store_remove(store, &iter);
  }
}


//------------------------------------------------------------------
// Scan events for upcoming events
//-------------------------------------------------------------------

gchar* scan_events_for_upcoming_events() {

   gchar *result_str;
   
   result_str="";
	
	GDateTime *dt;    
    dt = g_date_time_new_now_local();
    
    //gint year = g_date_time_get_year(dt);
    //gint month =g_date_time_get_month(dt);    
    //gint day = g_date_time_get_day_of_month(dt); 
    //g_print("scan: current year = %d\n", year);
    //g_print("scan: current month = %d\n", month);
    //g_print("scan: current day = %d\n", day);
    
    GDateTime *dtplus;
    gint scan_days=m_ahead;
    gchar *weekday;
    gint dow; //day of week
    gint year;
    gint month;   
    gint day;
    gint size=0;
    gchar *starthour_str;
	gchar *startmin_str;
	gint hour;
	gint min;
	gint morning;
    
    for(int d=0; d<=scan_days; d++) {
		
		dtplus =(GDateTime *)g_date_time_add_days(dt,d);		
		dow =g_date_time_get_day_of_week(dtplus);
		
		switch (dow) {
		case 1:
		weekday=" Monday";
		break;
		case 2:
		weekday=" Tuesday";
		break;
		case 3:
		weekday=" Wednesday";
		break;
		case 4:
		weekday=" Thursday";
		break;
		case 5:
		weekday=" Friday";
		break;
		case 6:
		weekday=" Saturday";
		break;
		case 7:
		weekday=" Sunday";
		break;
		default:
		weekday=" Unknown day";
		break;
	    }//switch dow
		
	year = g_date_time_get_year(dtplus);
    month =g_date_time_get_month(dtplus);    
    day = g_date_time_get_day_of_month(dtplus);
    
    size = db_get_number_of_rows_year_month_day(year, month, day);
    
    //g_print("Scan: size %d at loop d =%d\n",size,d);
    
    if (size ==0) {		
	 result_str= g_strconcat(result_str, " No events for ", weekday, ". ", NULL);		
	} //if	
	else {
	
	result_str= g_strconcat(result_str, " Events for ", weekday, ". ", NULL);
	
	Event event_array[size];
	db_get_all_events_year_month_day(event_array, year, month, day, size);
	
	Event evt;
	
	for(int i=0; i<size; i++) {	
		evt=event_array[i];	
		
		result_str=g_strconcat(result_str, evt.title,  ".  ", NULL);
		
		hour =evt.startHour;
		min=evt.startMin;
		
		if(evt.isAllday) {
		result_str= g_strconcat(result_str, " This is an all day event. ", NULL);				
		}
		else{ //not allday 
		morning =0; //check am/pm
				
		if (hour >= 12)
		{
			morning = 1;		
			if (hour > 12) hour = hour-12;
		}       		
		
		if (hour == 0) {
			morning = 2;
			hour = hour + 12;
		}
		
		switch (morning) {
			
			case 0:
			//g_print("hour = %d min = %d a.m.\n", hour,min);
			starthour_str = g_strdup_printf("%i", hour);
		    if(min>0) startmin_str = g_strdup_printf("%i", min); else startmin_str ="";
		    //startmin_str = g_strdup_printf("%i", min);			 
		    result_str= g_strconcat(result_str, " starts at ", starthour_str, " ",startmin_str, " A.M. ", NULL);	
			break;
			
			case 1:
			//g_print("hour = %d min = %d p.m.\n", hour,min);
			starthour_str = g_strdup_printf("%i", hour);
		    if(min>0) startmin_str = g_strdup_printf("%i", min); else startmin_str ="";
		    //startmin_str = g_strdup_printf("%i", min);		 
		    result_str= g_strconcat(result_str, " starts at ", starthour_str, " ",startmin_str, " P.M. ",NULL);			
			break;
			
			case 2:
			//g_print("hour = %d min = %d a.m.\n", hour,min);
			starthour_str = g_strdup_printf("%i", hour);
		    if(min>0) startmin_str = g_strdup_printf("%i", min); else startmin_str ="";	
		    //startmin_str = g_strdup_printf("%i", min);	
		    result_str= g_strconcat(result_str, " starts at ", starthour_str, " ",startmin_str, " A.M. ", NULL);
			break;
			
			default:
			//g_print("hour = %d min = %d UNKNOWN\n", hour,min);
			break;			
		} //switch morning				
		}//else not allday event
		
		result_str=g_strconcat(result_str,"  ", evt.description, ".  ", NULL);
		
		if(evt.priority ==1) { //low priority	
		result_str= g_strconcat(result_str, " This is a low priority event.", 	NULL);	
		}
		else if(evt.priority ==2) { //medium priority	
		result_str= g_strconcat(result_str, " This is a medium priority event.", 	NULL);	
		}
		else if(evt.priority ==3) { // high priority	
		result_str= g_strconcat(result_str, " This is a high priority event.", 	NULL);	
		}	
							
	}//for evts

	
    }//else have events
		
	}//for scandays
     
   
    //g_free(starthour_str); //valgrind leak -why??
   // g_free(startmin_str);
    g_date_time_unref (dt);
	g_date_time_unref (dtplus);
	
	return result_str;
	

}

//------------------------------------------------------------------
// Start up
//------------------------------------------------------------------

static void startup (GtkApplication *application)
{
	GMenu *main_menu;
	GMenu *file_menu;
	GMenu *edit_menu;
	GMenu *tools_menu;
	GMenu *help_menu;
	GMenuItem *item;
    
    main_menu = g_menu_new(); //main menu
    file_menu = g_menu_new(); //file menu
    edit_menu = g_menu_new(); //edit menu
    tools_menu = g_menu_new(); //tools menu    
    help_menu = g_menu_new(); //help menu
         
    // Create new menu item
    //item = g_menu_item_new("New", "app.new");
    //g_menu_append_item(file_menu,item);
	//g_object_unref(item);
	
	
	item = g_menu_item_new("Import", "app.import");    
    g_menu_insert_item(file_menu,1,item);    
    g_object_unref(item);
  
    item = g_menu_item_new("Export", "app.export");    
    g_menu_insert_item(file_menu,2,item);    
    g_object_unref(item);
     
    item = g_menu_item_new("Quit", "app.quit");
    g_menu_append_item(file_menu,item);
	g_object_unref(item);
	
	item = g_menu_item_new("Options", "app.options");
    g_menu_append_item(edit_menu,item);
	g_object_unref(item);
	
	item = g_menu_item_new("Read Events", "app.upcoming");
    g_menu_append_item(tools_menu,item);
	g_object_unref(item);
	
	item = g_menu_item_new("Goto Today (Spacebar)", "app.gotoday");
    g_menu_append_item(tools_menu,item);
	g_object_unref(item);
	
	item = g_menu_item_new("Delete All Events", "app.delete");
    g_menu_append_item(edit_menu,item);
	g_object_unref(item);
	
	item = g_menu_item_new("About", "app.about");
    g_menu_append_item(help_menu,item);
	g_object_unref(item); 
        
    //insert  menu into main menu          
    g_menu_insert_submenu(main_menu, 0, "File", G_MENU_MODEL(file_menu));
    g_menu_append_submenu(main_menu,"Edit", G_MENU_MODEL(edit_menu));
    g_menu_append_submenu(main_menu,"Tools", G_MENU_MODEL(tools_menu));
    g_menu_append_submenu(main_menu,"Help", G_MENU_MODEL(help_menu));
	//create the actions
    //create_actions(application);
        
    gtk_application_set_menubar(application, G_MENU_MODEL(main_menu));
    g_object_unref(main_menu);
    g_object_unref(file_menu);
    g_object_unref(tools_menu);
    g_object_unref(help_menu);     
	
}


/* signal handler for "toggled" signal of the CheckButton */
static void check_button_allday_toggled_callbk (GtkToggleButton *toggle_button, gpointer user_data)
{
 	
 	GtkWidget *spin_button_start_hour;
 	GtkWidget *spin_button_start_minutes; 
 	GtkWidget *spin_button_end_hour;
 	GtkWidget *spin_button_end_minutes;	
 	
 	
 	spin_button_start_hour =g_object_get_data(G_OBJECT(user_data), "cb_allday_spin_start_hour_key");
	spin_button_start_minutes =g_object_get_data(G_OBJECT(user_data), "cb_allday_spin_start_min_key"); 
	spin_button_end_hour= g_object_get_data(G_OBJECT(user_data), "cb_allday_spin_end_hour_key");
	spin_button_end_minutes= g_object_get_data(G_OBJECT(user_data), "cb_allday_spin_end_min_key");
	
 	
 	if (gtk_toggle_button_get_active(toggle_button)){		
	//g_print("Toggled callbk: Allday TRUE\n");
	
	gtk_widget_set_sensitive(spin_button_start_hour,FALSE);
	gtk_widget_set_sensitive(spin_button_start_minutes,FALSE);
	gtk_widget_set_sensitive(spin_button_end_hour,FALSE);
	gtk_widget_set_sensitive(spin_button_end_minutes,FALSE);
	
	
	}     
	else{	
	//g_print("Toggled callbk: Allday FALSE\n");
	
	gtk_widget_set_sensitive(spin_button_start_hour,TRUE);
	gtk_widget_set_sensitive(spin_button_start_minutes,TRUE);
	gtk_widget_set_sensitive(spin_button_end_hour,TRUE);
	gtk_widget_set_sensitive(spin_button_end_minutes,TRUE);
	}
  
}

static void callbk_combo_box_priority_on_changed(GtkComboBox *widget,  gpointer  user_data)
{
	GtkComboBox *combo_box_priorities = widget;
	gchar *priority_str = gtk_combo_box_text_get_active_text (GTK_COMBO_BOX_TEXT(combo_box_priorities));   
	//g_print ("Chosen priority = %s\n", priority_str);
	g_free (priority_str);
}

//---------------------------------------------------------------------
// Reload events on textview and list
//---------------------------------------------------------------------

static void reload_events(GtkWidget *widget, int year, int month, int day ) {

GtkWidget *window = (GtkWidget *) widget;
   
   if( !GTK_IS_WINDOW(window)) {	  
	 // g_print("The provided parameter is not a GtkWindow\n");
	  return;
  }
 
  // g_print("RELOADING EVENTS ON TEXT VIEW\n");
   GtkWidget *text_view = g_object_get_data(G_OBJECT(window), "textview-key"); //object-key association  
   GtkWidget *list = g_object_get_data(G_OBJECT(window), "list-key"); //object-key association
    //GtkCalendar *calendar =g_object_get_data(G_OBJECT(window), "calendar-key");
   
  remove_all(list, NULL); 
  GtkTextBuffer *text_buffer;
  gchar *output_str;  	 
  text_buffer = gtk_text_buffer_new (NULL);   
  output_str ="";
  gtk_text_buffer_set_text (text_buffer, output_str, -1);	//CLEAR
  gtk_text_view_set_buffer(GTK_TEXT_VIEW(text_view), text_buffer); 
  
  int row_number = db_get_number_of_rows_year_month_day(year, month, day);
 // g_print("RELOADING EVENTS after db insert to append inserted event \n");	
  Event event_array[row_number];		 
  db_get_all_events_year_month_day(event_array, year, month, day,row_number);
  
  Event evt;	
  for(int i=0; i<row_number; i++) {		
	  evt=event_array[i];	
	 
	 GDateTime  *time_start;
	 gchar     *time_start_str;
	 time_start = g_date_time_new_local(evt.year,evt.month,evt.day,evt.startHour, evt.startMin,0.0);         
	 time_start_str = g_date_time_format (time_start, "%l:%M %P");
	 
	 GDateTime  *time_end;
	 gchar     *time_end_str;
	 time_end = g_date_time_new_local(evt.year,evt.month,evt.day,evt.endHour, evt.endMin,0.0);         
	 time_end_str = g_date_time_format (time_end, "%l:%M %P");
	 
	 output_str= g_strconcat(output_str, evt.title, "\n", NULL);
	 
	 if(evt.isAllday) {
	 output_str= g_strconcat(output_str, "This is an all day event.\n", NULL);	 
	 }
	 else {	 
	 output_str= g_strconcat(output_str, time_start_str, " to ", time_end_str, "\n", NULL); 
	 }	 
	 //Two new lines for spacing -option?
	 output_str= g_strconcat(output_str, evt.description, "\n", "\n", NULL);
	 
	 gtk_text_buffer_set_text (text_buffer, output_str, -1);	
	 gtk_text_view_set_buffer(GTK_TEXT_VIEW(text_view), text_buffer);	
	 add_to_list(list, evt.id,evt.title);
	 //free		 
	 g_date_time_unref (time_start);
	 g_date_time_unref (time_end);
  }
}

//---------------------------------------------------------------------
// Update event
//---------------------------------------------------------------------

static void check_button_delete_toggled_callbk (GtkToggleButton *toggle_button, gpointer user_data)
{
 	if (gtk_toggle_button_get_active(toggle_button)){		
	//g_print("Toggled delete TRUE\n");	
	}     
	else{	
	//g_print("Toggled delete FALSE\n");
	}  
}


void callbk_etalker_update_event_response(GtkDialog *dialog, gint response_id,  gpointer  user_data) 
{
	
	
	if(!GTK_IS_DIALOG(dialog)) {	  
	//g_print("Not a dialog -returning\n");
	return;
	}
	
	Event *event =(Event*) user_data;
	gint id =event->id;
	
	//g_print("Callbk update event: id = %d\n",id);
	
	//get data
	GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "dialog-window-key"); 
	GtkWidget *entry_title = g_object_get_data(G_OBJECT(dialog), "entry-title-key");  
    GtkWidget *combo_box_priority= g_object_get_data(G_OBJECT(dialog), "combo-box-priority-key");  
    GtkWidget *text_view_description= g_object_get_data(G_OBJECT(dialog), "text-view-description-key");
    GtkTextBuffer *buffer_description;   
    GtkTextIter range_start, range_end;
	GtkWidget *spin_button_start_hour= g_object_get_data(G_OBJECT(dialog), "spin-start-hour-key");  
    GtkWidget *spin_button_start_minutes= g_object_get_data(G_OBJECT(dialog), "spin-start-minutes-key");       
    GtkWidget *spin_button_end_hour= g_object_get_data(G_OBJECT(dialog), "spin-end-hour-key");	
	GtkWidget *spin_button_end_minutes= g_object_get_data(G_OBJECT(dialog), "spin-end-minutes-key"); 
    GtkWidget *check_button_allday= g_object_get_data(G_OBJECT(dialog), "check-button-allday-key");  
    GtkWidget *check_button_delete= g_object_get_data(G_OBJECT(dialog), "check-button-delete-key");
    GtkWidget *list = g_object_get_data(G_OBJECT(dialog), "dialog-list-key");
    GtkTreeSelection *selection;
    
    //GPOINTER_TO_INT()
    gint year = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "dialog-year-key")); 
    gint month = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "dialog-month-key"));  
    gint day= GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "dialog-day-key"));  
    //gint year = g_object_get_data(G_OBJECT(dialog), "dialog-year-key"); 
    //gint month = g_object_get_data(G_OBJECT(dialog), "dialog-month-key");  
    //gint day= g_object_get_data(G_OBJECT(dialog), "dialog-day-key");  
 
	
	switch (response_id)
	{
	case 1:
	
	//g_print("The Ok button clicked\n"); 	
	
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(list));
	
	//CHECK DELETE request
	
	if(gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(check_button_delete)) ==1) {
	//g_print("Delete requested with id = %d\n", id);	
	db_delete_row(id);	//DELETE  
	//event deleted no longer exists so cant get data from it
	remove_item(GTK_WIDGET(list), selection); 	
	reload_events(window, year,month, day); //everything
	gtk_widget_destroy(GTK_WIDGET(dialog));
	return; //return after delete	
	}
	
	//Get changes
	//const gchar *title_str = gtk_entry_get_text(GTK_ENTRY(entry_title));
	//event->title=g_strconcat(title_str, NULL); //asignment to array type error
	//event->title =g_strdup(title_str); //asignment to array type error
	//g_sprintf(event->title,title_str);
	//sprintf(event->title,title_str);
	
	if (gtk_entry_get_text_length(GTK_ENTRY(entry_title))>0) {
	const gchar *tmp_str = gtk_entry_get_text(GTK_ENTRY(entry_title));
	//sprintf(event->title,tmp_str);
	strcpy(event->title,tmp_str);
	}
	else {
	//sprintf(event->title,"No title set");
	strcpy(event->title,"No title set");
	}	 
		
	
	event->priority = gtk_combo_box_get_active(GTK_COMBO_BOX (combo_box_priority));	
		 
	buffer_description=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view_description));		
	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(buffer_description), &range_start);
	range_end = range_start;
	gtk_text_iter_forward_chars(&range_end, 1000); 
	const gchar* description_str = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer_description),&range_start,&range_end,FALSE);

	//gint len = strlen(description_str);
	//g_print("length of description_str = %d\n",len);
	strcpy(event->description,description_str); 
		
	event->startHour =gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_button_start_hour));
	event->startMin=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_button_start_minutes));		 
	event->endHour =gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_button_end_hour));
	event->endMin=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_button_end_minutes));	
	
	if ((event->endHour<event->startHour) ||
	(event->endHour==event->startHour && event->endMin < event->startMin))
	{
	event->endHour =event->startHour;
	event->endMin =event->startMin;
	}
	
	event->isAllday=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(check_button_allday));
	
	if(event->isAllday) {	
	event->startHour =0;
	event->startMin=0;		 
	event->endHour =23;
	event->endMin=59;
	}
	
	
	db_update_event(event);	
    reload_events(window, year, month, day );	
	
	g_free(event);
	//g_free(title_str); //title is const?
	//g_free(description_str);  
	gtk_widget_destroy(GTK_WIDGET(dialog));
	break;
	
	case 2:
	//g_print("The Cancel button was pressed\n"); 
	g_free(event); 
	gtk_widget_destroy(GTK_WIDGET(dialog)); 
	break;
	case 3:
	g_free(event);
	gtk_widget_destroy(GTK_WIDGET(dialog));
	break;
	}
	
}

//static void etalker_update_event(GtkWidget *widget) {
static void etalker_update_event(GtkWidget *widget, gpointer data) {

 GtkWidget *window = (GtkWidget *) widget;
 Event *event =(Event*) data;
   
   if( !GTK_IS_WINDOW(window)) {	  
	  //g_print("The widget is not a GtkWindow\n");
	  return;
  }
 
    //GtkWidget *text_view = g_object_get_data(G_OBJECT(window), "textview-key"); //object-key association  
    GtkWidget *list = g_object_get_data(G_OBJECT(window), "list-key"); //object-key association
    //GtkCalendar *calendar =g_object_get_data(G_OBJECT(window), "calendar-key");
    
    //gint id =event->id;
    //GtkTreeSelection *selection;
    
    GtkWidget *dialog;    
    GtkWidget *container;	
	
	//Title Entry
	GtkWidget *label_title;
	GtkWidget *entry_title;	
	GtkWidget *box_title;
	
	//Priority	
	GtkWidget *label_priority;
	GtkWidget *combo_box_priority;
	GtkWidget *box_priority;
	
	
	
	//Description
	GtkWidget *label_description;
	GtkWidget *text_view_description;
    GtkWidget *scrolled_window_description;
    GtkTextBuffer *buffer_description;
    GtkWidget *box_description;
   // GtkTextIter start_iter, end_iter;

	//All day
	GtkWidget *check_button_allday;


	//Start time
	GtkWidget *label_start_time;
	GtkWidget *label_start_colon;
	GtkWidget *spin_button_start_hour;
	GtkWidget *spin_button_start_minutes;
	GtkWidget *box_start_time;
	
	//End time
	GtkWidget *label_end_time;
	GtkWidget *label_end_colon;
	GtkWidget *spin_button_end_hour;
	GtkWidget *spin_button_end_minutes;
	GtkWidget *box_end_time;
	
	//Delete event
	GtkWidget *check_button_delete;
		
			
   // gint response;
    
    GDateTime *date;
    gchar *date_str;
    date = g_date_time_new_local(event->year,event->month,event->day,0,0,0.0);
    date_str =g_date_time_format(date, "%d-%m-%Y");  //%d-%m-%Y
    g_date_time_unref (date);
    
    gchar *update_event_str;  
	update_event_str= g_strconcat("Update Event on ",date_str, NULL);
        
    
    //Create dialog
    dialog= gtk_dialog_new();
    //callbacks   
    g_signal_connect(dialog,"response",G_CALLBACK(callbk_etalker_update_event_response),event);
    //g_signal_connect(dialog,"close",G_CALLBACK(callbk_event_dialog_close),NULL); 
        
    gtk_window_set_title (GTK_WINDOW (dialog), update_event_str);  
    
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(window));  
    //gtk_window_set_title(GTK_WINDOW(dialog),"Update Event");
    gtk_widget_set_size_request(dialog, 300,200);
    gtk_window_set_modal(GTK_WINDOW(dialog),TRUE);  
    gtk_dialog_add_button(GTK_DIALOG(dialog),"Ok",1);  //OK response_id=1
    gtk_dialog_add_button(GTK_DIALOG(dialog),"Cancel",2); //Cancel response_id=2
    gtk_dialog_set_default_response(GTK_DIALOG(dialog),2); //default response cancel
           
	//--------------------------------------------------------
	// Title entry
	//---------------------------------------------------------
	label_title =gtk_label_new("Event Title: ");  
	entry_title =gtk_entry_new(); 
	gtk_entry_set_max_length(GTK_ENTRY(entry_title),30); 
	box_title=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_title),label_title,FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box_title),entry_title,TRUE, TRUE, 0);
	 
	gtk_entry_set_text(GTK_ENTRY(entry_title), event->title);
	  
	  //---------------------------------------------------------------
	  // Priority combobox
	  //------------------------------------------------------------------
	  
	  label_priority =gtk_label_new("Priority: ");  
	  /* Create the combo box and append your string values to it. */
	  combo_box_priority = gtk_combo_box_text_new ();
	  const char *priority[] = {"None", 
	  "Low", 
	  "Medium", 
	  "High"};
	  
	  
	  /* G_N_ELEMENTS is a macro which determines the number of elements in an array.*/ 
	  for (int i = 0; i < G_N_ELEMENTS (priority); i++){
	  gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box_priority), priority[i]);
	  }
	  
	  gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box_priority),event->priority);	
	  g_signal_connect (combo_box_priority,"changed", G_CALLBACK (callbk_combo_box_priority_on_changed), NULL);
	  box_priority=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	  gtk_box_pack_start(GTK_BOX(box_priority),label_priority,FALSE, FALSE, 0);
	  gtk_box_pack_start(GTK_BOX(box_priority),combo_box_priority,TRUE, TRUE, 0);
	
	
	//---------------------------------------------------------
	// Description entry
	//----------------------------------------------------------     
	label_description =gtk_label_new("Description: ");   
	buffer_description = gtk_text_buffer_new (NULL); 
	text_view_description = gtk_text_view_new_with_buffer (buffer_description); 
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view_description),TRUE);  
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view_description), GTK_WRAP_WORD); 
	scrolled_window_description = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window_description), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);      
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window_description),GTK_SHADOW_ETCHED_IN);  
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window_description), 5);  
	gtk_container_add (GTK_CONTAINER (scrolled_window_description), text_view_description); 
	box_description=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_description),label_description,FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box_description),scrolled_window_description,TRUE, TRUE, 0);
	
	gtk_text_buffer_set_text (buffer_description, event->description, -1);  
	
	
	
		
	//---------------------------------------------------------------
	// All day checkbox
	//---------------------------------------------------------------
		
	
	
	//--------------------------------------------------------
	// Start time spin buttons
	//--------------------------------------------------------- 
	label_start_time =gtk_label_new("Start Time (24 hour) ");
	label_start_colon=gtk_label_new(" : ");
	
	spin_button_start_hour = gtk_spin_button_new_with_range(0.0,23.0,1.0); //23 hours	
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_button_start_hour),0);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_button_start_hour),1.0,23.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button_start_hour),TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_start_hour),event->startHour);	
	g_signal_connect(spin_button_start_hour, "output", G_CALLBACK (on_output), NULL);
	
	
	spin_button_start_minutes=gtk_spin_button_new_with_range(0.0,59.0,1.0);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_button_start_minutes),0);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_button_start_minutes),1.0,10.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button_start_minutes),TRUE);	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_start_minutes),event->startMin);	
	g_signal_connect(spin_button_start_minutes, "output", G_CALLBACK (on_output), NULL);		
	
	box_start_time=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_start_time),label_start_time,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_start_time),spin_button_start_hour,TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box_start_time),label_start_colon,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_start_time),spin_button_start_minutes,TRUE, TRUE, 5);
		
	//--------------------------------------------------------
	// End time spin buttons
	//---------------------------------------------------------  
	label_end_time =gtk_label_new("End Time (24 hour) ");
	label_end_colon=gtk_label_new(" : ");
	
	spin_button_end_hour = gtk_spin_button_new_with_range(0.0,23.0,1.0); //23 hours	
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_button_end_hour),0);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_button_end_hour),1.0,23.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button_end_hour),TRUE);	
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_end_hour),event->endHour);	
	g_signal_connect(spin_button_end_hour, "output", G_CALLBACK (on_output), NULL);
	
	spin_button_end_minutes=gtk_spin_button_new_with_range(0.0,59.0,1.0);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_button_end_minutes),0);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_button_end_minutes),1.0,10.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button_end_minutes),TRUE);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_end_minutes), event->endMin);	
	g_signal_connect(spin_button_end_minutes, "output", G_CALLBACK (on_output), NULL);	
	
	box_end_time=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_end_time),label_end_time,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_end_time),spin_button_end_hour,TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box_end_time),label_end_colon,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_end_time),spin_button_end_minutes,TRUE, TRUE, 5);
		
	//---------------------------------------------------------------
	// All day checkbox
	//---------------------------------------------------------------
	
	check_button_allday = gtk_check_button_new_with_label ("All Day Event");
	
	 gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button_allday), event->isAllday); 
    
    if(event->isAllday) {
		gtk_widget_set_sensitive(spin_button_start_hour,FALSE);
	    gtk_widget_set_sensitive(spin_button_start_minutes,FALSE);
	    gtk_widget_set_sensitive(spin_button_end_hour,FALSE);
	    gtk_widget_set_sensitive(spin_button_end_minutes,FALSE);
	}
	else {
		gtk_widget_set_sensitive(spin_button_start_hour,TRUE);
		gtk_widget_set_sensitive(spin_button_start_minutes,TRUE);
		gtk_widget_set_sensitive(spin_button_end_hour,TRUE);
		gtk_widget_set_sensitive(spin_button_end_minutes,TRUE);
	}
	
	 g_object_set_data(G_OBJECT(check_button_allday), "cb_allday_spin_start_hour_key",spin_button_start_hour);
	 g_object_set_data(G_OBJECT(check_button_allday), "cb_allday_spin_start_min_key",spin_button_start_minutes); 
	 g_object_set_data(G_OBJECT(check_button_allday), "cb_allday_spin_end_hour_key",spin_button_end_hour);
	 g_object_set_data(G_OBJECT(check_button_allday), "cb_allday_spin_end_min_key",spin_button_end_minutes);
	
	 
       
    g_signal_connect_swapped (GTK_TOGGLE_BUTTON (check_button_allday), "toggled", 
						G_CALLBACK (check_button_allday_toggled_callbk), check_button_allday);
						
   
	
	
	
	
	
	// Delete check button
	check_button_delete = gtk_check_button_new_with_label ("Delete Event");
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button_delete), FALSE);   
    g_signal_connect (GTK_TOGGLE_BUTTON (check_button_delete), "toggled", G_CALLBACK (check_button_delete_toggled_callbk), NULL);
	
	//Data
	
	 //set up dialog data connections	
	 //g_object_set_data() makes a connection between two objects
	 g_object_set_data(G_OBJECT(dialog), "dialog-window-key",window); 
	 g_object_set_data(G_OBJECT(dialog), "entry-title-key",entry_title);  
     g_object_set_data(G_OBJECT(dialog), "combo-box-priority-key",combo_box_priority);  
     g_object_set_data(G_OBJECT(dialog), "text-view-description-key",text_view_description);	
	 g_object_set_data(G_OBJECT(dialog), "spin-start-hour-key",spin_button_start_hour);  
     g_object_set_data(G_OBJECT(dialog), "spin-start-minutes-key",spin_button_start_minutes); 
     g_object_set_data(G_OBJECT(dialog), "spin-end-hour-key",spin_button_end_hour);	
	 g_object_set_data(G_OBJECT(dialog), "spin-end-minutes-key",spin_button_end_minutes);  
     g_object_set_data(G_OBJECT(dialog), "check-button-allday-key",check_button_allday);
     g_object_set_data(G_OBJECT(dialog), "check-button-delete-key",check_button_delete);
     g_object_set_data(G_OBJECT(dialog), "dialog-list-key",list);
     // GtkWidget *list = g_object_get_data(G_OBJECT(window), "list-key");
     
     g_object_set_data(G_OBJECT(dialog), "dialog-year-key",GINT_TO_POINTER(event->year)); 
     g_object_set_data(G_OBJECT(dialog), "dialog-month-key",GINT_TO_POINTER(event->month));  
     g_object_set_data(G_OBJECT(dialog), "dialog-day-key",GINT_TO_POINTER(event->day));  
    
     //g_object_set_data(G_OBJECT(dialog), "year-key",event->year); 
     //g_object_set_data(G_OBJECT(dialog), "month-key",event->month);  
     //g_object_set_data(G_OBJECT(dialog), "day-key",event->day);  
	 
	
	//--------------------------------------------------------------
	// Dialog PACKING
	//--------------------------------------------------------------  
	container = gtk_dialog_get_content_area (GTK_DIALOG(dialog));	
	gtk_container_add(GTK_CONTAINER(container),box_title);
	gtk_container_add(GTK_CONTAINER(container),box_priority);			
	gtk_container_add (GTK_CONTAINER (container), box_description);	
	gtk_container_add(GTK_CONTAINER(container),check_button_allday);
	gtk_container_add(GTK_CONTAINER(container),box_start_time);		
	gtk_container_add(GTK_CONTAINER(container),box_end_time);
	gtk_container_add(GTK_CONTAINER(container),check_button_delete);
	gtk_widget_show_all(dialog);    
	
  
}


//---------------------------------------------------------------------
// New event
//---------------------------------------------------------------------

//--------------------------------------------------------------------
// New event close callback
//---------------------------------------------------------------------

static void callbk_event_dialog_close(GtkDialog *dialog,
					     G_GNUC_UNUSED gpointer   user_data)  
{
	if( !GTK_IS_DIALOG(dialog)) {	  
	//g_print("Not a dialog -returning\n");
	return;
	}	
	//g_print("callbk_event_dialog_close\n");
	gtk_widget_destroy(GTK_WIDGET(dialog));
} 

//--------------------------------------------------------------------
// New event response callback
//---------------------------------------------------------------------


void callbk_etalker_new_event_response(GtkDialog *dialog, gint response_id,  gpointer  user_data) 
{
	Event *event =user_data;
	
	//g_print("Year = %d\n", event->year);
	
	if(!GTK_IS_DIALOG(dialog)) {	  
	//g_print("Not a dialog -returning\n");
	return;
	}
	
	//get data
	GtkWidget *window = g_object_get_data(G_OBJECT(dialog), "dialog-window-key"); 
	GtkWidget *entry_title = g_object_get_data(G_OBJECT(dialog), "entry-title-key");  
    GtkWidget *combo_box_priority= g_object_get_data(G_OBJECT(dialog), "combo-box-priority-key");  
    GtkWidget *text_view_description= g_object_get_data(G_OBJECT(dialog), "text-view-description-key");
    GtkTextBuffer *buffer_description;    
    GtkTextIter range_start, range_end;    
	GtkWidget *spin_button_start_hour= g_object_get_data(G_OBJECT(dialog), "spin-start-hour-key");  
    GtkWidget *spin_button_start_minutes= g_object_get_data(G_OBJECT(dialog), "spin-start-minutes-key");       
    GtkWidget *spin_button_end_hour= g_object_get_data(G_OBJECT(dialog), "spin-end-hour-key");	
	GtkWidget *spin_button_end_minutes= g_object_get_data(G_OBJECT(dialog), "spin-end-minutes-key"); 
    GtkWidget *check_button_allday= g_object_get_data(G_OBJECT(dialog), "check-button-allday-key");  
    //GtkWidget *check_button_delete= g_object_get_data(G_OBJECT(dialog), "check-button-delete-key");
    //GPOINTER_TO_INT()
    gint year = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "dialog-year-key")); 
    gint month = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "dialog-month-key"));  
    gint day= GPOINTER_TO_INT(g_object_get_data(G_OBJECT(dialog), "dialog-day-key"));  
 
	
switch (response_id)
{
	case 1:
	//g_print("Ok button pressed\n");
	event->year =year;
	event->month=month;
	event->day=day;
  
	//sprintf(event->title,"No title set");
	
	
	//if (tmp_str != NULL) {
	
	if (gtk_entry_get_text_length(GTK_ENTRY(entry_title))>0) {
	const gchar *tmp_str = gtk_entry_get_text(GTK_ENTRY(entry_title));
	//sprintf(event->title,tmp_str);
	strcpy(event->title,tmp_str);
	}
	else {
	//sprintf(event->title,"No title set");
	strcpy(event->title,"No title set");
	}	 
  
	gint active_id = gtk_combo_box_get_active (GTK_COMBO_BOX(combo_box_priority));
  
	  switch (active_id)
	  {
		  case 0:
		  event->priority=0; //None
		  break;
		  case 1:
		  event->priority=1; //Low
		  break;
		  case 2:
		  event->priority=2; //Medium
		  break;
		  case 3:
		  event->priority=3; //High
		  break;		  
		  default:
		  event->priority=0; 
		  break; 
	  }
	  
	  
	 
	buffer_description=gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view_description));	
	
	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(buffer_description), &range_start);
	range_end = range_start;
	gtk_text_iter_forward_chars(&range_end, 1000); 
	const gchar* description_str = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(buffer_description),&range_start,&range_end,FALSE);

	//gint len = strlen(description_str);
	//g_print("length of description_str = %d\n",len);
	strcpy(event->description,description_str); 

	 
   
  event->startHour =gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_button_start_hour));
  event->startMin=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_button_start_minutes));		 
  event->endHour =gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_button_end_hour));
  event->endMin=gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(spin_button_end_minutes));	
  
  if ((event->endHour<event->startHour) ||
  (event->endHour==event->startHour && event->endMin < event->startMin))
  {
	  event->endHour =event->startHour;
	  event->endMin =event->startMin;
  }
  
  event->isAllday=gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(check_button_allday));   
  //g_print("New Event OK: event.isAllday = %d\n",event->isAllday);
  
  if(event->isAllday) 
  {	
	  event->startHour =0;
	  event->startMin=0;		 
	  event->endHour =23;
	  event->endMin=59;
  }
  
  //g_print("Inserting event into db for %d-%d-%d\n", day, month,year);  //autoincrement insert 
  int id = db_insert_event(event);	 //?? deref
  //g_print("inserted id: %d\n", id);	
  g_free(event);
  
  //Reload events
  reload_events(window, year, month, day );
  
  
  gtk_widget_destroy(GTK_WIDGET(dialog));
  break;
  
	case 2:
	//g_print("Cancel button pressed\n");
	g_free(event);
	gtk_widget_destroy(GTK_WIDGET(dialog));
	break;
	
	case 3:
	//g_print("Other action taken\n");
	g_free(event);
	gtk_widget_destroy(GTK_WIDGET(dialog));
	break;
}
	
	
}


static void etalker_new_event(GtkWidget *widget, guint year, guint month, guint day ) {

 GtkWidget *window = (GtkWidget *) widget;
   
   if( !GTK_IS_WINDOW(window)) {	  
	 // g_print("Widget is not a GtkWindow\n");
	  return;
  }
 
    //GtkWidget *text_view = g_object_get_data(G_OBJECT(window), "textview-key"); //object-key association  
   // GtkWidget *list = g_object_get_data(G_OBJECT(window), "list-key"); //object-key association
    //GtkCalendar *calendar =g_object_get_data(G_OBJECT(window), "calendar-key");
    
    GtkWidget *dialog;    
    GtkWidget *container;	
	
	//Title Entry
	GtkWidget *label_title;
	GtkWidget *entry_title;	
	GtkWidget *box_title;
	
	//Priority	
	GtkWidget *label_priority;
	GtkWidget *combo_box_priority;
	GtkWidget *box_priority;
	
	
	//Description
	GtkWidget *label_description;
	GtkWidget *text_view_description;
    GtkWidget *scrolled_window_description;
    GtkTextBuffer *buffer_description;
    GtkWidget *box_description;
    //GtkTextIter start_iter, end_iter;

	//All day
	GtkWidget *check_button_allday;
	//GtkWidget *check_button_delete; //not used with new event


	//Start time
	GtkWidget *label_start_time;
	GtkWidget *label_start_colon;
	GtkWidget *spin_button_start_hour;
	GtkWidget *spin_button_start_minutes;
	GtkWidget *box_start_time;
	
	//End time
	GtkWidget *label_end_time;
	GtkWidget *label_end_colon;
	GtkWidget *spin_button_end_hour;
	GtkWidget *spin_button_end_minutes;
	GtkWidget *box_end_time;
		
		
	//Event  *event = calloc(1, sizeof(Event)); 	
	 //Event event;	
	 Event *event =g_malloc(sizeof(Event));	//free in callbk
    //gint response;
    
    //gchar *year_str = g_strdup_printf("%i", year);
    //gchar *month_str = g_strdup_printf("%i", month);
    //gchar *day_str = g_strdup_printf("%i", day);
    
    GDateTime *date;
    gchar *date_str;
    date = g_date_time_new_local(year,month,day,0,0,0.0);
    date_str =g_date_time_format(date, "%d-%m-%Y");  //%d-%m-%Y
    g_date_time_unref (date);
    
    gchar *new_event_str;  
	new_event_str= g_strconcat("New Event on ",date_str, NULL);
        
    //Create dialog
    dialog= gtk_dialog_new(); 
    gtk_window_set_title (GTK_WINDOW (dialog), new_event_str); 
   
   //callbacks   
    g_signal_connect(dialog,"response",G_CALLBACK(callbk_etalker_new_event_response),event);
    g_signal_connect(dialog,"close",G_CALLBACK(callbk_event_dialog_close),NULL); 
    
    gtk_window_set_transient_for(GTK_WINDOW(dialog),GTK_WINDOW(window));  
    //gtk_window_set_title(GTK_WINDOW(dialog),"New Event");
    gtk_widget_set_size_request(dialog, 300,200);
    gtk_window_set_modal(GTK_WINDOW(dialog),TRUE);  
    gtk_dialog_add_button(GTK_DIALOG(dialog),"Ok",1);  //Ok button has response_id=1
    gtk_dialog_add_button(GTK_DIALOG(dialog),"Cancel",2); //Cancel button has response_id =2
    //Note all other actions have response_id =3
    gtk_dialog_set_default_response(GTK_DIALOG(dialog),2); //default response cancel
    
   
        
	//--------------------------------------------------------
	// Title entry
	//---------------------------------------------------------
	label_title =gtk_label_new("Event Title: ");  
	entry_title =gtk_entry_new(); 
	gtk_entry_set_max_length(GTK_ENTRY(entry_title),30); 
	box_title=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_title),label_title,FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box_title),entry_title,TRUE, TRUE, 0);
	  
	//---------------------------------------------------------------
	// Priority combobox
	//------------------------------------------------------------------
	
	label_priority =gtk_label_new("Priority: ");  
	/* Create the combo box and append your string values to it. */
    combo_box_priority = gtk_combo_box_text_new ();
    const char *priority[] = {"None", 
	"Low", 
	"Medium", 
	"High"};
	
	
	/* G_N_ELEMENTS is a macro which determines the number of elements in an array.*/ 
	for (int i = 0; i < G_N_ELEMENTS (priority); i++){
	gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (combo_box_priority), priority[i]);
	}
	
	gtk_combo_box_set_active (GTK_COMBO_BOX (combo_box_priority), 0);	
	g_signal_connect (combo_box_priority,"changed", G_CALLBACK (callbk_combo_box_priority_on_changed), NULL);
	box_priority=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_priority),label_priority,FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box_priority),combo_box_priority,TRUE, TRUE, 0);
	

	
	//---------------------------------------------------------
	// Description entry
	//----------------------------------------------------------     
	label_description =gtk_label_new("Description: ");   
	buffer_description = gtk_text_buffer_new (NULL); 
	text_view_description = gtk_text_view_new_with_buffer (buffer_description); 
	gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view_description),TRUE);  
	gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view_description), GTK_WRAP_WORD); 
	scrolled_window_description = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window_description), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);      
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window_description),GTK_SHADOW_ETCHED_IN);  
	gtk_container_set_border_width (GTK_CONTAINER (scrolled_window_description), 5);  
	gtk_container_add (GTK_CONTAINER (scrolled_window_description), text_view_description); 
	box_description=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_description),label_description,FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(box_description),scrolled_window_description,TRUE, TRUE, 0);
		
	
	//--------------------------------------------------------
	// Start time spin buttons
	//--------------------------------------------------------- 
	label_start_time =gtk_label_new("Start Time (24 hour) ");
	label_start_colon=gtk_label_new(" : ");
	spin_button_start_hour = gtk_spin_button_new_with_range(0.0,23.0,1.0); //23 hours	
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_button_start_hour),0);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_button_start_hour),1.0,23.0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_start_hour),0.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button_start_hour),TRUE);		
	g_signal_connect(spin_button_start_hour, "output", G_CALLBACK (on_output), NULL);
	
	spin_button_start_minutes=gtk_spin_button_new_with_range(0.0,59.0,1.0);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_button_start_minutes),0);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_button_start_minutes),1.0,10.0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_start_minutes),0.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button_start_minutes),TRUE);	
	
	g_signal_connect(spin_button_start_minutes, "output", G_CALLBACK (on_output), NULL);		
	
	box_start_time=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_start_time),label_start_time,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_start_time),spin_button_start_hour,TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box_start_time),label_start_colon,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_start_time),spin_button_start_minutes,TRUE, TRUE, 5);
		
	//--------------------------------------------------------
	// End time spin buttons
	//---------------------------------------------------------  
	label_end_time =gtk_label_new("End Time (24 hour) ");
	label_end_colon=gtk_label_new(" : ");
	spin_button_end_hour = gtk_spin_button_new_with_range(0.0,23.0,1.0); //23 hours	
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_button_end_hour),0);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_button_end_hour),1.0,23.0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_end_hour),0.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button_end_hour),TRUE);
	g_signal_connect(spin_button_end_hour, "output", G_CALLBACK (on_output), NULL);
	
	spin_button_end_minutes=gtk_spin_button_new_with_range(0.0,59.0,1.0);
	gtk_spin_button_set_digits(GTK_SPIN_BUTTON(spin_button_end_minutes),0);
	gtk_spin_button_set_increments(GTK_SPIN_BUTTON(spin_button_end_minutes),1.0,10.0);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin_button_end_minutes),0.0);
	gtk_spin_button_set_numeric(GTK_SPIN_BUTTON(spin_button_end_minutes),TRUE);		 
	g_signal_connect(spin_button_end_minutes, "output", G_CALLBACK (on_output), NULL);	
	
		
	box_end_time=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
	gtk_box_pack_start(GTK_BOX(box_end_time),label_end_time,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_end_time),spin_button_end_hour,TRUE, TRUE, 5);
	gtk_box_pack_start(GTK_BOX(box_end_time),label_end_colon,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_end_time),spin_button_end_minutes,TRUE, TRUE, 5);
	
	
	//---------------------------------------------------------------
	// All day checkbox
	//---------------------------------------------------------------
	//gboolean f_all_day =FALSE;
	check_button_allday = gtk_check_button_new_with_label ("All Day Event");
	
	 g_object_set_data(G_OBJECT(check_button_allday), "cb_allday_spin_start_hour_key",spin_button_start_hour);
	 g_object_set_data(G_OBJECT(check_button_allday), "cb_allday_spin_start_min_key",spin_button_start_minutes); 
	 g_object_set_data(G_OBJECT(check_button_allday), "cb_allday_spin_end_hour_key",spin_button_end_hour);
	 g_object_set_data(G_OBJECT(check_button_allday), "cb_allday_spin_end_min_key",spin_button_end_minutes);
	
	 
       
    g_signal_connect_swapped (GTK_TOGGLE_BUTTON (check_button_allday), "toggled", 
						G_CALLBACK (check_button_allday_toggled_callbk), check_button_allday);
						
	 gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button_allday), FALSE);  
	
	
	//check_button_delete = gtk_check_button_new_with_label ("Delete Event");
    //gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check_button_delete), FALSE);   
	
	
	
	 //set up dialog data connections	
	 //g_object_set_data() makes a connection between two objects
	 g_object_set_data(G_OBJECT(dialog), "dialog-window-key",window); 
	 g_object_set_data(G_OBJECT(dialog), "entry-title-key",entry_title);  
     g_object_set_data(G_OBJECT(dialog), "combo-box-priority-key",combo_box_priority);  
     g_object_set_data(G_OBJECT(dialog), "text-view-description-key",text_view_description);	
	 g_object_set_data(G_OBJECT(dialog), "spin-start-hour-key",spin_button_start_hour);  
     g_object_set_data(G_OBJECT(dialog), "spin-start-minutes-key",spin_button_start_minutes); 
     g_object_set_data(G_OBJECT(dialog), "spin-end-hour-key",spin_button_end_hour);	
	 g_object_set_data(G_OBJECT(dialog), "spin-end-minutes-key",spin_button_end_minutes);  
     g_object_set_data(G_OBJECT(dialog), "check-button-allday-key",check_button_allday);
    // g_object_set_data(G_OBJECT(dialog), "check-button-delete-key",check_button_delete);
     
     g_object_set_data(G_OBJECT(dialog), "dialog-year-key",GINT_TO_POINTER(year)); 
     g_object_set_data(G_OBJECT(dialog), "dialog-month-key",GINT_TO_POINTER(month));  
     g_object_set_data(G_OBJECT(dialog), "dialog-day-key",GINT_TO_POINTER(day));  
    
		
	//--------------------------------------------------------------
	// Dialog PACKING
	//--------------------------------------------------------------  
	container = gtk_dialog_get_content_area (GTK_DIALOG(dialog));	
	gtk_container_add(GTK_CONTAINER(container),box_title);
	gtk_container_add(GTK_CONTAINER(container),box_priority);			
	gtk_container_add (GTK_CONTAINER (container), box_description);	
	gtk_container_add(GTK_CONTAINER(container),check_button_allday);
	gtk_container_add(GTK_CONTAINER(container),box_start_time);		
	gtk_container_add(GTK_CONTAINER(container),box_end_time);
	gtk_widget_show_all(dialog); 
	
	//g_free(event);
 
   
}


//---------------------------------------------------------------------
// Calendar callbacks
//----------------------------------------------------------------------
static void calendar_month_changed_callbk(GtkCalendar *calendar, gpointer user_data) {
	
   guint day, month, year;

   gtk_calendar_get_date(GTK_CALENDAR (calendar), &year, &month, &day); 
   
   gtk_calendar_clear_marks(GTK_CALENDAR (calendar));    
   
   //g_print("Month changed: year = %d month = %d\n",year,month+1);
   
   gint row_number = db_get_number_of_rows_month_year(month+1, year);
     
   //g_print("Number of month events = %d\n",row_number); 
	
	if (row_number ==0) {		
		//g_print("No events for month\n");
		return;
	}
	else {	 
	 //g_print("Fetching month changed events\n");	
	 Event event_array[row_number];		 
	 db_get_all_events_year_month(event_array, year, month+1, row_number);
	 //mark up calendar
	 Event evt;	
	for(int i=0; i<row_number; i++) {		
		evt=event_array[i];		//
		guint day = evt.day;		
		gtk_calendar_mark_day(calendar, day);
	} 	 
   }
}


static void calendar_day_selected_double_click_callbk(GtkCalendar *calendar, gpointer user_data)
{
   guint day, month, year;

   gtk_calendar_get_date(GTK_CALENDAR (calendar), &year, &month, &day);    
   //g_print("Double Click Date: Year = %d Month =%d Day =%d\n", year, month+1,day);   
   //g_print("Call New Event Dialog for year=%d month=%d day=%d\n", year, month+1,day);
   
   GtkWidget *window = (GtkWidget *) user_data;
   
   if( !GTK_IS_WINDOW(window)) {	  
	  //g_print("The provided parameter is not a GtkWindow\n");
	  return;
  }
  
  etalker_new_event(window, year, month+1, day);
 
  //reload_events(window, year, month+1, day);
  calendar_month_changed_callbk(calendar, NULL);
  
}

static gpointer thread_speak_func(gpointer user_data)
{     
    gchar *text =user_data;
    //g_print("speaking day events %s\n", text);
    gchar * command_str ="espeak --stdout";
    gchar *m_str = g_strdup_printf("%i", m_speed); 
    gchar *speed_str ="-s ";
    speed_str= g_strconcat(speed_str,m_str, NULL);   
    command_str= g_strconcat(command_str," '",speed_str,"' "," '",text,"' ", "| aplay", NULL);
    system(command_str);
    g_mutex_unlock (&lock); //thread mutex unlock 
    return NULL;   
}


static void calendar_day_selected_callbk(GtkCalendar *calendar, gpointer user_data)
{
   
   //GtkWidget *window = (GtkWidget *) user_data;
   //GtkWidget *window = g_object_get_data(G_OBJECT(user_data), "window-key"); //object-key association
         
   GtkWidget *text_view = (GtkWidget *) user_data;  
   GtkWidget *list = g_object_get_data(G_OBJECT(calendar), "calendar-list-key"); //object-key association     
   GtkWidget *label_date =g_object_get_data(G_OBJECT(calendar), "calendar-label-date-key"); 
   
   remove_all(list, NULL);
   GtkTextBuffer *text_buffer;
   gchar *output_str; 
   //GtkTextIter start_iter, end_iter;
      
   text_buffer = gtk_text_buffer_new (NULL);   
   output_str ="";
   
   gtk_text_buffer_set_text (text_buffer, output_str, -1);	//clear
   gtk_text_view_set_buffer(GTK_TEXT_VIEW(text_view), text_buffer);
	
   guint day, month, year;
   gtk_calendar_get_date(GTK_CALENDAR (calendar), &year, &month, &day);
    
    GDateTime *dt;
    gchar *dt_format;
    dt = g_date_time_new_local(year,month+1,day,0,0,0.0);   // get date    
    dt_format = g_date_time_format(dt, "%a %e %b %Y");	
    gtk_label_set_xalign(GTK_LABEL(label_date),0.5);
	gtk_label_set_text(GTK_LABEL(label_date), dt_format); 	
    
  
   //g_print("Date: Year = %d Month =%d Day =%d\n", year, month+1,day);   
   //g_print("Get number of events for year=%d month=%d day=%d\n", year, month+1,day);
	
	int row_number = db_get_number_of_rows_year_month_day(year, month+1, day);
	//g_print("Number of events = %d\n",row_number); 
	
	if (row_number ==0) {		
		//g_print("No events\n");
		//speak_str=g_strconcat(speak_str, "No events today",NULL);
	}
	else {	 
	 //g_print("Day selected fetching events\n");	
	 Event event_array[row_number];	
	 db_get_all_events_year_month_day(event_array, year, month+1, day,row_number);	
	//print out	
	//printf("Events returned from db call.....\n");	
	Event evt;	
	for(int i=0; i<row_number; i++) {	
		evt=event_array[i];
		//printf("\n");
		//printf("-----------------------------------------------\n");		
		//printf("id = %d\n", evt.id);
		//printf("title = %s\n", evt.title);
		//printf("category = %s\n", evt.category);	
		//printf("description = %s\n", evt.description);					
		//printf("year = %d\n", evt.year);
		//printf("month = %d\n", evt.month);
		//printf("day = %d\n",evt.day);
		//printf("startHour = %d\n", evt.startHour);
		//printf("startMin = %d\n", evt.startMin);
		//printf("endHour = %d\n", evt.endHour);
		//printf("endMin = %d\n", evt.endMin);
		//printf("isAllday = %d\n",evt.isAllday);		
		
		//gchar *starthour_str = g_strdup_printf("%i", evt.startHour);
		//gchar *startmin_str = g_strdup_printf("%i", evt.startMin);
		//gchar *endhour_str = g_strdup_printf("%i", evt.endHour);
		//gchar *endmin_str = g_strdup_printf("%i", evt.endMin);
		
		         
         GDateTime  *time_start;
         gchar     *time_start_str;
         time_start = g_date_time_new_local(evt.year,evt.month,evt.day,evt.startHour, evt.startMin,0.0);         
         time_start_str = g_date_time_format (time_start, "%l:%M %P");
         
         GDateTime  *time_end;
         gchar     *time_end_str;
         time_end = g_date_time_new_local(evt.year,evt.month,evt.day,evt.endHour, evt.endMin,0.0);         
         time_end_str = g_date_time_format (time_end, "%l:%M %P");
         
		output_str= g_strconcat(output_str, evt.title, "\n", NULL);
		
		if(evt.isAllday) {
			output_str= g_strconcat(output_str, "This is an all day event\n", NULL);
			
		}
		else {
			
		output_str= g_strconcat(output_str, time_start_str, " to ", time_end_str, "\n", NULL); 		
		
			
		}
		
		//Two new lines for spacing -option?
		
		output_str= g_strconcat(output_str, evt.description, "\n", "\n", NULL);
		
		
		//speak_str=g_strconcat(speak_str, evt.title, " at ", time_start_str, "  ", NULL); 
		//output_str= g_strconcat(output_str, 
		//evt.title, "\n", 
		//starthour_str, ":",startmin_str, " to ", 		
		//endhour_str, ":",endmin_str, "\n", 		
		//evt.description, "\n", "\n",  
		//NULL);	
		
		gtk_text_buffer_set_text (text_buffer, output_str, -1);	
		gtk_text_view_set_buffer(GTK_TEXT_VIEW(text_view), text_buffer);
		
		//add_to_list(list,GINT_TO_POINTER(evt.id),evt.title);
		add_to_list(list,evt.id,evt.title);
		
		//free
		g_date_time_unref(dt); 
		g_date_time_unref (time_start);
		g_date_time_unref (time_end);
	}			
	}//else 	
	calendar_month_changed_callbk(calendar,NULL);
	   
}

//---------------------------------------------------------------------
//Delete all events 
//---------------------------------------------------------------------

static void callbk_delete_all_response(GtkDialog *dialog, gint response_id, gpointer  user_data)								 
{
	
	GtkWindow *window = user_data; //window data
	GtkTextBuffer *text_buffer;
    gchar *output_str;  
    
    GtkWidget *text_view = g_object_get_data(G_OBJECT(window), "textview-key"); //object-key association
	GtkWidget *list =g_object_get_data(G_OBJECT(window), "list-key");
	GtkCalendar *calendar =g_object_get_data(G_OBJECT(window), "calendar-key");
	 //g_object_set_data(G_OBJECT(window), "textview-key",text_view);  
     //g_object_set_data(G_OBJECT(window), "list-key",list);  
     //g_object_set_data(G_OBJECT(window), "calendar-key",calendar);
     
     if (response_id == GTK_RESPONSE_OK) {
     //Delete everything
     db_delete_all();
     db_reset_sequence();
     //clear 
     text_buffer = gtk_text_buffer_new (NULL);   
     output_str ="";
     gtk_text_buffer_set_text (text_buffer, output_str, -1);	//clear
     gtk_text_view_set_buffer(GTK_TEXT_VIEW(text_view), text_buffer);
     //clear list
     remove_all(list, NULL);
      //clear calendar marks
     calendar_month_changed_callbk(calendar,NULL);
   
     gtk_widget_destroy (GTK_WIDGET (dialog));
     }
     
     else if (response_id == GTK_RESPONSE_CANCEL) {
		 
		 //g_print ("Cancel delete all\n");		
		gtk_widget_destroy(GTK_WIDGET (dialog));
	 }
	  else if (response_id == GTK_RESPONSE_DELETE_EVENT) {
          //g_print ("dialog closed or cancelled\n");
          
		  gtk_widget_destroy(GTK_WIDGET (dialog));
	 }
	
}

static void delete_events_callbk(GtkWidget *window){	
	
	GtkWidget *message_dialog;
	
	//g_print("Delete all events callback ...\n");
	
	message_dialog = gtk_message_dialog_new (GTK_WINDOW(window), GTK_DIALOG_MODAL, 
                                            GTK_MESSAGE_WARNING, 
                                            GTK_BUTTONS_OK_CANCEL, 
                                            "Delete All Events?");
	
	g_signal_connect (GTK_DIALOG (message_dialog), "response",   G_CALLBACK (callbk_delete_all_response), window);
	
	gtk_widget_show(message_dialog); 	
		
}

//--------------------------------------------------------------------
// Speak at startup
//---------------------------------------------------------------------
static void speak_at_startup() {
	
	
	GThread *thread_speak_startup; 
	
	gchar* message_speak =scan_events_for_upcoming_events();  
		
	if(m_speaking) {	
	g_mutex_lock (&lock);
    thread_speak_startup = g_thread_new(NULL, thread_speak_func, message_speak);   
	}
	
	g_thread_unref (thread_speak_startup);	

}



gboolean callbk_window_on_key_press(GtkWidget * widget, GdkEvent * event,
                    gpointer data)
{
	GtkWidget *window = (GtkWidget *) widget;
   
   if( !GTK_IS_WINDOW(window)) {	  
   //g_print("Widget is not a GtkWindow\n");
   return FALSE;
   }
   
   GtkCalendar *calendar =g_object_get_data(G_OBJECT(window), "calendar-key");
   GtkWidget *label_date =g_object_get_data(G_OBJECT(window), "label-date-key");   
   GdkEventKey key = event->key;
   
   if(key.keyval ==GDK_KEY_space) {
	   
		   
	   GDateTime *dt; 
	   gchar *dt_format;
       dt = g_date_time_new_now_local();   // get local time     
       dt_format = g_date_time_format(dt, "%a %e %b %Y");	   
	   
	   gtk_label_set_xalign(GTK_LABEL(label_date),0.5);
	   gtk_label_set_text(GTK_LABEL(label_date), dt_format); 	
	   
	   gint year = g_date_time_get_year(dt);
	   gint month =g_date_time_get_month(dt);    
	   gint day = g_date_time_get_day_of_month(dt); 
	   //g_print("scan: current year = %d\n", year);
	  // g_print("scan: current month = %d\n", month);
	  // g_print("scan: current day = %d\n", day);
	   
	   gtk_calendar_select_month (calendar, month-1, year);	   
	   gtk_calendar_select_day (calendar, day);	   
	   reload_events(window, year, month,day);
	   g_date_time_unref(dt); 
   }
return TRUE;
	
}



//---------------------------------------------------------------------
// activate
//---------------------------------------------------------------------

static void activate (GtkApplication *app, gpointer user_data)
{
  GtkWidget *window;
  GtkWidget *calendar;
  GtkWidget *label_date;
  GtkWidget *paned;
  GtkWidget *text_view;
  GtkWidget *scrolled_window;
  GtkTextBuffer *buffer_text_view;
  
  GtkWidget *box_pane1;
  GtkWidget *box_pane2;
  
  GtkWidget *list;
  
  GDateTime *dt;
  gchar *dt_format;
  dt = g_date_time_new_now_local();   // get local time     
  dt_format = g_date_time_format(dt, "%a %e %b %Y");
  
  //-----------------------------------------------------------------
  // check configuration
  //-----------------------------------------------------------------
  
  //g_print("Config: speaking = %d\n", m_speaking ? "true" : "false");
 // g_print("Config: speaking = %d\n", m_speaking);
//  g_print("Config: speed = %d\n", m_speed);
  
  
  //----------------------------------------------------------------
  //set up window
  //------------------------------------------------------------------
  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Talking Events Assistant");
  gtk_window_set_default_size (GTK_WINDOW (window), 800, 400);
  gtk_window_set_icon_name (GTK_WINDOW (window), "x-office-calendar");
  
  g_signal_connect(G_OBJECT (window), "key_press_event", G_CALLBACK(callbk_window_on_key_press), NULL);
  
  //Set up list view    
  list = gtk_tree_view_new(); // a widget for displaying both trees and lists    
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(list), TRUE);    
  g_object_set(G_OBJECT(list), "activate-on-single-click", FALSE, NULL);
  g_signal_connect(GTK_TREE_VIEW(list), "row-activated", G_CALLBACK(list_row_activated_callbk), window);    
  init_list(list);
 
 
  //-------------------------------------------------------------------
  // set up text view
  //-------------------------------------------------------------------
  
  buffer_text_view = gtk_text_buffer_new (NULL); //text being edited  
  text_view = gtk_text_view_new_with_buffer (buffer_text_view);
  gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view),FALSE); 
  
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view), GTK_WRAP_WORD);   
 
  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window), 
                                  GTK_POLICY_AUTOMATIC, 
                                  GTK_POLICY_AUTOMATIC); 
  
  gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
  gtk_container_set_border_width (GTK_CONTAINER (scrolled_window), 5);
 
  
  //-------------------------------------------------------------------
  // Set up calendar
  //--------------------------------------------------------------------
  
  //set up calendar
  calendar = gtk_calendar_new();
 // gtk_calendar_set_display_options(calendar,GTK_CALENDAR_SHOW_DAY_NAMES); 
  
  g_signal_connect(GTK_CALENDAR(calendar), "day-selected", G_CALLBACK(calendar_day_selected_callbk), text_view);
  g_signal_connect(GTK_CALENDAR(calendar), "day-selected-double-click", G_CALLBACK(calendar_day_selected_double_click_callbk), window);
  g_signal_connect(GTK_CALENDAR(calendar), "month-changed", G_CALLBACK(calendar_month_changed_callbk), NULL);
  
 
  //g_signal_connect(GTK_CALENDAR(calendar), "day-selected", G_CALLBACK(calendar_day_selected_callbk), list);
  //g_signal_connect(GTK_CALENDAR(calendar), "day-selected-double-click", G_CALLBACK(calendar_day_selected_double_click_callbk), window);
  //g_signal_connect(GTK_CALENDAR(calendar), "month-changed", G_CALLBACK(calendar_month_changed_callbk), NULL);
   //---------------------------------------------------------------
	// Date label
	//--------------------------------------------------------------
	
	label_date=gtk_label_new("");	
	gtk_label_set_xalign(GTK_LABEL(label_date),0.5);
	gtk_label_set_text(GTK_LABEL(label_date), dt_format); 	
	
 
	 //----------------------------------------------------------------
	 // Set up connections
	 //-----------------------------------------------------------------
	 //g_object_set_data() makes a connection between two objects
	 g_object_set_data(G_OBJECT(window), "textview-key",text_view);  
     g_object_set_data(G_OBJECT(window), "list-key",list);  
     g_object_set_data(G_OBJECT(window), "calendar-key",calendar);
     g_object_set_data(G_OBJECT(window), "label-date-key",label_date);
     
	 g_object_set_data(G_OBJECT(calendar), "calendar-list-key",list);
	 g_object_set_data(G_OBJECT(calendar), "calendar-textview-key",text_view); 
	 g_object_set_data(G_OBJECT(calendar), "calendar-label-date-key",label_date);   
	 
	 g_object_set_data(G_OBJECT(list), "treeview-textview-key",text_view);
	 g_object_set_data(G_OBJECT(list), "treeview-calendar-key",calendar);   	
	
	//------------------------------------------------------------------
	// Menu Actions
	//------------------------------------------------------------------
	
	GSimpleAction *export_action;	
	export_action=g_simple_action_new("export",NULL); //app.export
	g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(export_action)); //make visible
	//connect the exit_action activate signal to the exit_callbk function with application user data
	g_signal_connect(export_action, "activate",  G_CALLBACK(export_callbk), NULL);
	
	
	GSimpleAction *import_action;	
	import_action=g_simple_action_new("import",NULL); //app.import
	g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(import_action)); //make visible
	//connect the exit_action activate signal to the exit_callbk function with application user data
	g_signal_connect(import_action, "activate",  G_CALLBACK(import_callbk), calendar);
	
	
	GSimpleAction *exit_action;	
	exit_action=g_simple_action_new("quit",NULL); //app.quite
	g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(exit_action)); //make visible
	//connect the exit_action activate signal to the exit_callbk function with application user data
	g_signal_connect(exit_action, "activate",  G_CALLBACK(exit_callbk), app);
	
	GSimpleAction *options_action;	
	//options_action=g_simple_action_new("options",NULL); //action = app.options
	options_action=g_simple_action_new("options",NULL); 
	g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(options_action)); //make visible
	//connect the exit_action activate signal to the exit_callbk function with application user data
	g_signal_connect(options_action, "activate",  G_CALLBACK(options_callbk), window);
	
		
	GSimpleAction *action_upcoming_events;	
	//options_action=g_simple_action_new("options",NULL); //action = app.options
	action_upcoming_events=g_simple_action_new("upcoming",NULL); 
	g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(action_upcoming_events)); //make visible
	//connect the exit_action activate signal to the exit_callbk function with application user data
	g_signal_connect(action_upcoming_events, "activate",  G_CALLBACK(callbk_upcoming_events), window);
	
	GSimpleAction *gotoday_action;	
	//options_action=g_simple_action_new("options",NULL); //action = app.options
	gotoday_action=g_simple_action_new("gotoday",NULL); 
	g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(gotoday_action)); //make visible
	//connect the exit_action activate signal to the exit_callbk function with application user data
	g_signal_connect(gotoday_action, "activate",  G_CALLBACK(callbk_gotoday), window);
	
	
	GSimpleAction *delete_events_action;	
	delete_events_action=g_simple_action_new("delete",NULL); //action = app.event
	g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(delete_events_action)); //make visible
	//connect the exit_action activate signal to the exit_callbk function with application user data
	g_signal_connect_swapped(delete_events_action, "activate",  G_CALLBACK(delete_events_callbk), window);
	//g_signal_connect(delete_events_action, "activate",  G_CALLBACK(delete_events_callbk), calendar);
	
	
	GSimpleAction *about_action;	
	about_action=g_simple_action_new("about",NULL); //app.about
	g_action_map_add_action(G_ACTION_MAP(app), G_ACTION(about_action)); //make visible
	//connect the exit_action activate signal to the exit_callbk function with application user data
	g_signal_connect_swapped(about_action, "activate",  G_CALLBACK(callbk_about), window);
	
	
	
	
  
    box_pane1=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    gtk_box_pack_start(GTK_BOX(box_pane1),calendar,FALSE, FALSE, 2);
    gtk_box_pack_start(GTK_BOX(box_pane1),list,FALSE, FALSE, 2);
    
    box_pane2=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    //gtk_box_pack_start(GTK_BOX(box_pane2),scrolled_window,TRUE, TRUE, 5);
    gtk_box_pack_start(GTK_BOX(box_pane2),label_date,FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(box_pane2),scrolled_window,TRUE, TRUE, 5);	
	
	paned = gtk_paned_new (GTK_ORIENTATION_HORIZONTAL);
		

	//gtk_paned_add1 (GTK_PANED (paned), calendar);	
	//gtk_paned_add2 (GTK_PANED (paned), text_view);
	
	gtk_paned_add1 (GTK_PANED (paned), box_pane1);
	gtk_paned_add2 (GTK_PANED (paned), box_pane2);
	
	//GtkAllocation allocation;
	//gtk_widget_get_allocation (window, &allocation);
    gtk_paned_set_position(GTK_PANED (paned), 450); //window 800x400
    
	gtk_container_add (GTK_CONTAINER (window), paned);
	//gtk_container_add(GTK_CONTAINER(window), calendar);
	calendar_month_changed_callbk(GTK_CALENDAR(calendar),NULL);
	calendar_day_selected_callbk(GTK_CALENDAR(calendar), GTK_TEXT_VIEW(text_view));
	speak_at_startup();
	g_date_time_unref(dt); 
	//calendar_day_selected_callbk(calendar, list);
  //gtk_widget_show(calendar); 
  gtk_widget_show_all (window);
  
}

int main (int argc, char **argv)
{
    
  //setup database
  db_create_events_table();  
  //setup preferences
  config_initialize();
   
  GtkApplication *app;
  int status;

  app = gtk_application_new ("org.gtk.etalker", G_APPLICATION_FLAGS_NONE);
  
  g_signal_connect_swapped(app, "startup", G_CALLBACK (startup),app);
  g_signal_connect_swapped(app, "activate", G_CALLBACK (activate),app);
  
  
  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref(app);
  
  

  return status;
}
