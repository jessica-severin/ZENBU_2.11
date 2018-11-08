#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/mman.h>

#include <oscdb.h>

/*************************************
 * internal prototypes
 *************************************/
osc_object_t*     oscdb_parse_obj_line(oscdb_t* self, char* line, int line_len);
void              oscdb_mmap_idx(oscdb_t *self);

/*************************************
 * now all the subroutines
 *************************************/

oscdb_t* oscdb_new(void) {
  oscdb_t  *self;
  
  self = (oscdb_t*) malloc(sizeof(oscdb_t));
  bzero(self, sizeof(oscdb_t));

  self->max_lines = -1;
  self->debug = 0;
  self->startclock = clock();
  self->idx_fildes = 0;
  
  return self;  
}

void oscdb_init(oscdb_t *self, char* infile) {
  int        readCount;

  self->file_path = infile;

  if(self->buffer == NULL) {
    self->buffer = (char*)malloc(BUFSIZE);
    self->readbuf = (char*)malloc(READ_BUF_SIZE);  
  }
  bzero(self->buffer, BUFSIZE);

  if(self->data_fildes == 0) { 
    self->data_fildes = open(self->file_path, O_RDONLY, 0x755); 
  }

  //
  //prefill the buffers and get ready for processing
  //
  readCount = read(self->data_fildes, self->readbuf, READ_BUF_SIZE);
  self->buffer[0] = '\0';
  strncat(self->buffer, self->readbuf, readCount);
  //printf("new buffer length : %d\n", strlen(buffer));
  self->bufptr = self->buffer;
  self->bufend = self->buffer + strlen(self->buffer); /* points at the \0 added by strncat()*/
  self->seek_pos = 0;
  self->next_obj_id = 1; //object ids start at 1
}


/**
 * read the 'expression' file generated by the obj extraction process
 ***/
void oscdb_build_index(oscdb_t *self) {
  int        readCount;
  FILE       *fp_idx = NULL;
  long long  byteCount=0, bufferCount=0, seek_pos;
  double     runtime, rate;
  double     mbytes;
  char       *bufptr, *bufptr2, *bufend;
  int        objcount = 1;
  char       outpath[1024];
  //time_t     start_t = time(NULL);

  //makes sure everthing is initialized correctly
  lseek(self->data_fildes, 0, SEEK_SET);
  bzero(self->buffer, BUFSIZE);

  sprintf(outpath, "%s.eeidx", self->file_path);
  fp_idx = fopen(outpath, "w");
  
  printf("long : %lu bytes\n", sizeof(bufferCount));
  while(1) {
    readCount = read(self->data_fildes, self->readbuf, READ_BUF_SIZE);
    if(readCount<=0) { break; }

    bufferCount++;
    strncat(self->buffer, self->readbuf, readCount);
    //printf("new buffer length : %d\n", (int)strlen(self->buffer));
    bufptr = self->buffer;
    bufptr2 = self->buffer;
    bufend = self->buffer + strlen(self->buffer); /* points at the \0 added by strncat()*/

    while(bufptr2 < bufend) {
      while((bufptr2 < bufend) && (*bufptr2 != '\0') && (*bufptr2 != '\n')) { bufptr2++; }
      if(*bufptr2 == '\n') {
        *bufptr2 = '\0';
        seek_pos = byteCount + (bufptr - self->buffer);
        //fprintf(fp_out, "%s\n", bufptr);

        //fprintf(fp_out, "%s\n", bufptr);
        if((*bufptr!='#')&&(*bufptr!='>')) {
          fwrite(&seek_pos, sizeof(seek_pos), 1, fp_idx);
          if(self->debug > 1) { printf("object %d => %lld offset :: %s \n", objcount, seek_pos, bufptr); }
          objcount++;
        }

        bufptr2 = bufptr2+1;
        bufptr = bufptr2;
        
      }
    } //bottom of the while(bufptr2<bufend){} loop

    //update the bytecount since we are done with this buffer
    //printf("end of buffer\nstarting byteCount:%lld, readCount:%d, bufend:%d,  bufptr:%d\n", byteCount, readCount, (bufend-buffer), (bufptr-buffer));
    byteCount = byteCount + (bufptr - self->buffer);
    if(bufferCount % 1000 == 0) {
      printf("  read %1.3f Mbytes\t%d objs\n", ((double)(byteCount))/(1024.0*1024.0), objcount);
    }

    /* not an end of line means we ran off the buffer
       so we need to shift the rest of the buffer
       to the beginning and break out to read more
       of the file */
    strcpy(self->buffer, bufptr); //the end over, very efficient


  } //end of the while(1) loop
  lseek(self->data_fildes, 0, SEEK_SET);
  fclose(fp_idx);
  
  mbytes = ((double)(byteCount))/(1024.0*1024.0);
  runtime = (double)(clock() - self->startclock) / CLOCKS_PER_SEC;
  //runtime = (double)(time(NULL) - start_t);
  rate = objcount/runtime; 

  printf("just read %d objects\n", objcount);
  printf("just read %1.3f Mbytes\n", mbytes);
  printf("  %1.3f secs\n", (float)runtime);
  if(rate>1000000.0) {
    printf("  %1.3f mega objs/sec\n", rate/1000000.0); 
  } else if(rate>2000.0) {
    printf("  %1.3f kilo objs/sec\n", rate/1000.0); 
  } else {
    printf("  %1.3f objs/sec\n", rate); 
  }
  printf("  %1.3f mbytes/sec\n", mbytes/runtime); 
}


void oscdb_mmap_idx(oscdb_t *self) {
  off_t      idx_len;
  char       outpath[1024];
  void       *mmap_addr;
  
  if(self->data_fildes == 0) { 
    self->data_fildes = open(self->file_path, O_RDONLY, 0x755); 
  }
  if(self->idx_fildes == 0) {
    sprintf(outpath, "%s.eeidx", self->file_path);
    self->idx_fildes = open(outpath, O_RDONLY, 0x755);
  }
  idx_len = lseek(self->idx_fildes, 0, SEEK_END);  
  if(self->debug) { printf("index file %lld bytes long\n", (long long)idx_len); }
  lseek(self->idx_fildes, 0, SEEK_SET);
  
  mmap_addr =  mmap(NULL, idx_len, PROT_READ, MAP_PRIVATE, self->idx_fildes, 0);
  self->idx_mmap = mmap_addr;
  self->max_id = (idx_len / sizeof(long long));
  if(self->debug) { printf("index file max ID: %lld\n", (long long)self->max_id); }
}


/*************************************************************
*
* object access methods
*
*************************************************************/

/**
 * read the next object in the stream, probably already in the buffers
 ***/
osc_object_t* oscdb_next_object(oscdb_t *self) {
  int        readCount, line_len;
  char       *bufptr2;
  osc_object_t    *obj = NULL;

  bufptr2 = self->bufptr;
  while(1) {
    while(bufptr2 < self->bufend) {
      while((bufptr2 < self->bufend) && (*bufptr2 != '\0') && (*bufptr2 != '\n')) { bufptr2++; }
      if(*bufptr2 == '\n') {
        *bufptr2 = '\0';
        //fprintf(fp_out, "%s\n", bufptr);

        //
        // do things with the line buffer here
        //
        //printf("LINE %7lld : %10lld offset :: %s \n", self->next_obj_id, self->seek_pos, self->bufptr);
        line_len = (bufptr2 - self->bufptr) + 1;
        obj = oscdb_parse_obj_line(self, self->bufptr, line_len);
        self->seek_pos = self->seek_pos + line_len;
        self->bufptr = bufptr2+1;
        bufptr2 = bufptr2+1;

        if(obj) { 
          //if(self->debug) { eedb_print_tag(self, obj); }
          return obj;
        }
      }
    } //bottom of the while(bufptr2<bufend){} loop
    
    /* we ran off the self->buffer
       so we need to shift the rest of the buffer
       to the beginning and then read more
       of the file */
    strcpy(self->buffer, self->bufptr); //the end over with \0 char, very efficient

    readCount = read(self->data_fildes, self->readbuf, READ_BUF_SIZE);
    if(readCount<=0) { return NULL; }
    
    strncat(self->buffer, self->readbuf, readCount);
    //printf("new buffer length : %d\n", (int)strlen(self->buffer));
    self->bufptr = self->buffer;
    bufptr2 = self->bufptr;    
    self->bufend = self->buffer + strlen(self->buffer); /* points at the \0 added by strncat()*/
  }
  return NULL;
}


/**
 * random access an object in the stream.
 * always does a seek and refills the buffers
 * before generating the object
 ***/
osc_object_t* oscdb_access_object(oscdb_t *self, long long id) {
  ssize_t    readCount, line_len;
  char       *bufptr2;
  osc_object_t   *obj = NULL;
  
  if(self->idx_fildes == 0) { oscdb_mmap_idx(self); }

  if((id < 1) || (id > self->max_id)) { return NULL; }
  
  //
  // ok now the seek magick, got to love memory-mapped binary files
  //  
  self->next_obj_id = id;
  self->seek_pos = self->idx_mmap[id-1];
  if(self->debug>1) { printf("OBJECT %7lld : %10lld offset\n", id, self->seek_pos); }
  lseek(self->data_fildes, self->seek_pos, SEEK_SET);

  //
  // and now prepare the buffers
  //
  readCount = read(self->data_fildes, self->readbuf, 2048);
  self->buffer[0] = '\0';
  strncat(self->buffer, self->readbuf, readCount);
  //printf("new buffer length : %d\n", strlen(buffer));
  self->bufptr = self->buffer;
  self->bufend = self->buffer + strlen(self->buffer); /* points at the \0 added by strncat()*/

  bufptr2 = self->buffer;
  while((bufptr2 < self->bufend) && (*bufptr2 != '\0') && (*bufptr2 != '\n')) { bufptr2++; }

  if(*bufptr2 == '\n') {
    *bufptr2 = '\0';
    if(self->debug>1) { printf("LINE %7lld : %10lld offset :: %s \n", id, self->seek_pos, self->bufptr); }
    line_len = (bufptr2 - self->bufptr) + 1;
    obj = oscdb_parse_obj_line(self, self->bufptr, line_len);
    self->seek_pos = self->seek_pos + line_len;
    self->bufptr = bufptr2+1;
  }
  return obj;
}

/*************************************************************
*
* osc_object_t section: super class of all objects in the system
*
**************************************************************/

osc_object_t*  oscdb_parse_obj_line(oscdb_t* self, char* line, int line_len) {
  osc_object_t   *obj = NULL;
  char           *ptr, *p1;
  int            col=0;

  if(*line =='#') { return NULL; }
  if(*line =='>') { return NULL; }

  obj = (osc_object_t*)malloc(sizeof(osc_object_t));
  bzero(obj, sizeof(*obj));

  obj->id = self->next_obj_id++;
  obj->offset = self->seek_pos;
  obj->oscdb = self;

  obj->line_buffer =(char*)malloc(line_len+1);
  strcpy(obj->line_buffer, line);

  //segment the line into columns
  ptr = obj->line_buffer;
  p1 = ptr;
  obj->columns[col++] = p1;

  while(*ptr != '\0') {
    if(*ptr == '\t') {
      *ptr = '\0';
      ptr++;
      p1 = ptr;
      obj->columns[col++] = p1;
    } else {
      ptr++;
    }
  }
  
  return obj;
}



/*************************************
 *
 * osc_object section
 *
 *************************************/

void osc_object_delete(osc_object_t *obj) {
  void     *line;

  if(obj==NULL) { return; }
  
  line = obj->line_buffer;
  obj->line_buffer = NULL;
  free(line);
  
  free(obj);
}


void osc_object_parse_columns(osc_object_t *self) {
}


void osc_object_print(osc_object_t *self) {
  char *str;
  if(self == NULL) { return; }
  //printf("FEATURE(%lld) %s\n", ((osc_object_t*)self)->id, ((osc_object_t*)self)->line_buffer);
  str = osc_object_display_desc(self);
  printf("%s\n", str);
  free(str);
}



char* osc_object_display_desc(osc_object_t *self) {
  char *str = (char*)malloc(2048);
  snprintf(str, 2040, "OBJ(%15lld) [off:%15lld] %s", self->id, self->offset, self->line_buffer);
  return str;
}





