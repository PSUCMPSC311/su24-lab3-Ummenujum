#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

int is_mounted = 0;
int is_written = 0;


int mdadm_mount(void) {

  return 0;
}

int mdadm_unmount(void) {
 
  return 0;
}

int mdadm_write_permission(void){
 
  return 0;
}


int mdadm_revoke_write_permission(void){
	return 0;
}


int mdadm_read(uint32_t start_addr, uint32_t read_len, uint8_t *read_buf)  {
	return 0;
}



int mdadm_write(uint32_t start_addr, uint32_t write_len, const uint8_t *write_buf) {
	return 0;
}

