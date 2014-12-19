

#include "OiMemoryModel.h"
#include "elf32-tiny.h"
#include "InterpUtils.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

using namespace llvm;

uint64_t OiMemoryModel::LoadELF(const char *filename)
{ 
  Elf32_Ehdr    ehdr;
  Elf32_Shdr    shdr;
  Elf32_Phdr    phdr;
  int           fd;
  unsigned int  i;
  Elf32_Word    size = 0; 
  uint64_t      entry;

  //Open application
  if (!filename || ((fd = open(filename, 0)) == -1)) {
    report_fatal_error(strerror(errno));
    exit(EXIT_FAILURE);
  }

  //Test if it's an ELF file
  if ((read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) ||  // read header
      (strncmp((char *)ehdr.e_ident, ELFMAG, 4) != 0) ||          // test elf magic number
      0) {
    close(fd);
    return EXIT_FAILURE;
  }

  //Set start address
  entry = ehdr.e_entry;
  if (entry > TOTALSIZE) {
    report_fatal_error("the start address of the application is beyond model memory\n");
    close(fd);
    exit(EXIT_FAILURE);
  }

  if (ehdr.e_type == ET_EXEC) {
    //Get program headers and load segments
    for (i = 0; i < ehdr.e_phnum; i++) {
      unsigned int segment_type;

      //Get program headers and load segments
      lseek(fd, ehdr.e_phoff + ehdr.e_phentsize * i, SEEK_SET);
      if (read(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
        report_fatal_error("reading ELF program header\n");
        close(fd);
        exit(EXIT_FAILURE);
      }

      segment_type = phdr.p_type;
      
      switch(segment_type) {
      case PT_INTERP: { // Requesting program interpreter
        report_fatal_error("Unsupported: Dynamically linked object");
        close(fd);
        exit(EXIT_FAILURE);
        break;
      }
      case PT_LOAD: { // Loadable segment type - load dynamic segments as well
        Elf32_Addr p_vaddr = phdr.p_vaddr;
        Elf32_Word p_memsz = phdr.p_memsz;
        Elf32_Word p_filesz = phdr.p_filesz;
        Elf32_Off  p_offset = phdr.p_offset;
        
        //Error if segment greater then memory
        if (TOTALSIZE < p_vaddr + p_memsz) {
          report_fatal_error("not enough memory to load application.\n");
          close(fd);
          exit(EXIT_FAILURE);
        }
        
        //Set heap to the end of the segment
        if (heapPtr < p_vaddr + p_memsz) heapPtr = p_vaddr + p_memsz;

        //Load 
        lseek(fd, p_offset, SEEK_SET);
        if (read(fd, memory + p_vaddr, p_filesz) != (signed)p_filesz) {
          report_fatal_error("reading ELF LOAD segment.\n");
          close(fd);
          exit(EXIT_FAILURE);
        }
        memset(memory + p_vaddr + p_filesz, 0, p_memsz - p_filesz);
        break;
      }
      default:
        break;
      };
    }
  }
  else if (ehdr.e_type == ET_REL) {
    report_fatal_error("Unsupported: ELF relocatable file");
    exit(EXIT_FAILURE);
  }

  //Close file
  close(fd);
  return entry;
}

