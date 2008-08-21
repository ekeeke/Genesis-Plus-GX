
#ifndef _DOS_H_
#define _DOS_H_

#define LOGERROR 1

/* Function prototypes */
int load_file(char *filename, char *buf, int size);
int save_file(char *filename, char *buf, int size);
void dos_update_input(void);
void dos_update_audio(void);
void dos_update_palette(void);
void dos_update_video(void);
void init_machine(void);
void trash_machine(void);
void make_vdp_palette(void);
void dos_change_mode(void);
int check_key(int code);

#define update_input() dos_update_input()
#define MAX_INPUTS 8

/* options */
extern uint8 overscan;
extern uint8 use_480i;
extern uint8 FM_GENS;
extern uint8 hq_fm;
extern uint8 ssg_enabled;
extern double psg_preamp;
extern double fm_preamp;
extern uint8 boost;
extern uint8 region_detect;
extern uint8 sys_type[2];
extern uint8 force_dtack;
extern uint8 dmatiming;
extern uint8 vdptiming;

extern uint8 debug_on;
extern uint8 log_error;

#endif /* _DOS_H_ */

