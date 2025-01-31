#ifndef _MUIC_DEBUG_
#define _MUIC_DEBUG_

extern void muic_reg_log(u8 reg, u8 value, u8 rw);
extern void muic_print_reg_log(void);
extern void muic_read_reg_dump(muic_data_t *muic, char *mesg);
extern void muic_print_reg_dump(muic_data_t *pmuic);
extern void muic_show_debug_info(struct work_struct *work);

#endif
