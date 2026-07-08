#ifndef STORAGE_SD_H
#define STORAGE_SD_H

void init_sd(void);
void save_state(void);
void load_state(void);
void append_session_row(void);
void start_measurement(void);
void stop_measurement(void);
void resume_session_if_needed(void);

#endif
