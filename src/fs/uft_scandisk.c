
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int analyze(unsigned char* data,int len,long long fpos){
    unsigned int magic=*((unsigned int*)data);
    unsigned int magic2=*((unsigned int*)(data+4));
    //--------------- NTFS:INDX --------------------
    if(magic==0x58444E49){
        unsigned int used=*((unsigned int*)(data+28)); // Size of Index Node
        unsigned int size=*((unsigned int*)(data+32)); // Allocated_Size_of_Index_Node
        unsigned int vcn=*((unsigned int*)(data+16));  // Virtual Cluster Number (VCN) of the index entry
        size+=24;
        if(used<=size && size<=16384 && size>=1024 && !(size&511)){
            printf("%012llX: NTFS:INDX #%d  %d/%d\n",fpos,vcn,used,size);
            return 64;
        } else return 0; // invalid
    }
    //--------------- NTFS:FILE --------------------
    if(magic==0x454C4946){
        unsigned int used=*((unsigned int*)(data+24)); // Used entry size
        unsigned int size=*((unsigned int*)(data+28)); // Total entry size
        unsigned int mft=*((unsigned int*)(data+44));  // elvileg itt tarolja az mft szamat
        int flags=256*data[23]+data[22];
        if(used<=size && size<=16384 && size>=1024 && !(size&511)){
            printf("%012llX: NTFS:MFT #%d  %d/%d  f:%d\n",fpos,mft,used,size,flags);
            return 128;
        } else return 0; // invalid
    }
    //--------------- ZIP/DOCX --------------------
    if(magic==0x04034B50){
        int ver=256*data[5]+data[4];  // version, 20 for deflated, 10 for store
        int comp=256*data[9]+data[8]; // compression method, 8=deflated / 0=store
        unsigned int csize=*((unsigned int*)(data+18));
        unsigned int usize=*((unsigned int*)(data+22));
        printf("%012llX: ZIP v%d comp=%d %d/%d '%.*s'\n",fpos,ver,comp, csize,usize, data[26],data+30);
        return (comp==8 && ver==20) ? 1 : 0; // most zip files are v20 deflated
    }
    //--------------- OLE2/DOC --------------------
    if(magic==0xE011CFD0 && magic2==0xE11AB1A1){
        unsigned int v=*((unsigned int*)(data+24));
        printf("%012llX: OLE2 v%d.%d  0x%08X\n",fpos,data[26],data[24],v);
        //      26  OLE2 v3.59     2599  OLE2 v3.62        9  OLE2 v4.62
        return (v==0x0003003B || v==0x0003003E || v==0x0004003E) ? 1 : 0;
    }
    //------------------ RTF ----------------------
    if(magic==0x74725C7B && magic2==0x615C3166){      // "{\rtf1\a"
        printf("%012llX: RTF\n",fpos);
        return 1;
    }
    //---------------- SAV/SPSS -------------------
    if(magic==0x324C4624 && magic2==0x29232840){      // "$FL2@(#)"  24 46 4C 32 │ 40 28 23 29
        printf("%012llX: SAV %.56s\n",fpos,data+8);
        return 1;
    }
    //------------------ PDF ----------------------
//    if(magic==0x46445025 && data[4]==0x2D && data[6]==0x2E && data[7]>=0x30 && data[7]<=0x39){
    if(magic==0x46445025 && (magic2&0xF8FFFFFF)==0x302E312D){ // PDF-1.[0-7]
        printf("%012llX: %.7s\n",fpos,data+1);  // PDF  # 25 50 44 46 │ 2D 31 2E 37  
        return 1;
    }
    //------------------ PNG ----------------------
    if(magic==0x474E5089 && magic2==0x0A1A0A0D && !memcmp(data+12,"IHDR",4)){      // PNG (first chunk must be IHDR)
        printf("%012llX: PNG %dx%dx%d\n",fpos, 256*data[16+2]+data[16+3], 256*data[20+2]+data[20+3],data[24]);
        return 2;
    }
    //------------------ PSD ----------------------
    if(magic==0x53504238 && magic2==0x100){ // 8BPS   00 01 00 00
    // 0:  38 42 50 53 │ 00 01 00 00 │ 00 00 00 00 
    //12:  00 03
    //14:  00 00 25 0F
    //18:  00 00 34 2C
    //22:  00 08 
        printf("%012llX: PSD %dx%dx%d %dbpp\n",fpos, 256*data[20]+data[21], 256*data[16]+data[17],data[13],data[23]);
        return !data[12] && data[13] && !data[22] && (data[23]==1 || data[23]==8 || data[23]==16 || data[23]==32) ? 2 : 0; // bpp=1|8|16|32
    }
    //------------------ JPG ----------------------
    if( (magic&0xF0FFFFFF)==0xE0FFD8FF){   // FF D8 FF Ex
        int l=0; while(data[6+l]>=32 && data[6+l]<127) l++; // find ascii string len
        printf("%012llX: JPEG %02X:%.*s\n",fpos, data[3],l,data+6);
        // JPEG E0:JFIF | E1:Exif | E2:ICC_PROFILE | ED:Photoshop 3.0 | EE:Adobe
        return !memcmp(data+6,"JFIF",4) || !memcmp(data+6,"Exif",4) || !memcmp(data+6,"http",4) || !memcmp(data+6,"ICC_PROFILE",10) ? 2 : 0;
    }
    //---------------- MPV/MP4 --------------------
    if( magic2==0x70797466 && (magic&0xFFFFFF)==0 && data[3]>=16 ){   // 66 74 79 70  "ftyp"
        // First chunk must be of type "ftype" and has a sub-type at offset 8. MOV defined by sub-type which must be "qt ".
        int next=data[3]; if(!memcmp(data+next+4,"free",4) && !data[next] && !data[next+1] && !data[next+2]) next+=data[next+3]; // skip "free" chunk (WTF???)
        printf("%012llX: MOV:%.4s %.4s %d\n",fpos,data+8,data+next+4, next);
        return 2;
//        return !memcmp(data+next+4,"mdat",4) || !memcmp(data+next+4,"moov",4) ? 2 : 0;
    }
//    else printf("%12llX: %08X %08X %d\n",fpos,magic,magic2,12345);
    return 0;
}

int entropy(unsigned char* data,int len){
    unsigned int* p=(unsigned int*)data;
    len/=sizeof(unsigned int);
    int i=1;
    while(i<len && p[i]==p[0]) i++;
    return (i==len)?0:1;
}

#define BUFFCOUNT 128
#define BUFFSIZE (8*1024*1024)
unsigned char buffer[BUFFSIZE];

int main(int argc,char* argv[]){
  FILE* f=fopen(argc<=1 ? "/dev/nvme1n1p3" : argv[1],"rb"); if (!f) { perror("fopen"); return 1; }
  long long fpos=0;
  long long tote=0;
  int c=0;
  while(fread(buffer,BUFFSIZE,1,f)==1){
    int e=0,f=0;
    if(!((c++)%BUFFCOUNT)) fprintf(stderr,"\n%8d GB - ",(int)(fpos>>30));
    for(int bpos=0;bpos<BUFFSIZE;bpos+=512){
      f|=analyze(buffer+bpos,512,fpos);
      e+=entropy(buffer+bpos,512);
      fpos+=512;
    }
    if(f) // found known file format
//        fputc( (f&128)?'M': (f&64)?'I': (f&1)?'d': (f&2)?'p' : '?' ,stderr); // MFT/INDX/doc/pic
        fputc( (f&128)?'M': (f&64)?'I': (f&1)?'~': (f&2)?'#' : '?' ,stderr); // MFT/INDX/doc/pic
    else // raw data
        fputc( (e==0)?'0' : (e==BUFFSIZE/512)?'*' : '.' ,stderr); // null / full / mixed
    tote+=e;
  }
  fprintf(stderr,"\n%lld bytes / %lld sectors (%5.2f%% used)\n",fpos, fpos/512, 100.0*tote/(fpos/512));
}

