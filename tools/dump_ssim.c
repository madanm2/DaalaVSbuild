#include "vidinput.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#if !defined(M_PI)
# define M_PI (3.141592653589793238462643)
#endif
#include <string.h>
/*Yes, yes, we're going to hell.*/
#if defined(_WIN32)
#include <io.h>
#include <fcntl.h>
#endif
#include <ogg/os_types.h>
#include "getopt.h"



const char *optstring = "frsy";
const struct option options[]={
  {"frame-type",no_argument,NULL,'f'},
  {"raw",no_argument,NULL,'r'},
  {"summary",no_argument,NULL,'s'},
  {"luma-only",no_argument,NULL,'y'},
  {NULL,0,NULL,0}
};

static int show_frame_type;
static int summary_only;
static int luma_only;


#define KERNEL_SHIFT (8)
#define KERNEL_WEIGHT (1<<KERNEL_SHIFT)
#define KERNEL_ROUND ((1<<KERNEL_SHIFT)>>1)

static int gaussian_filter_init(unsigned **_kernel,double _sigma,int _max_len){
  unsigned *kernel;
  double    scale;
  double    nhisigma2;
  double    s;
  double    len;
  unsigned  sum;
  int       kernel_len;
  int       kernel_sz;
  int       ci;
  scale=1/(sqrt(2*M_PI)*_sigma);
  nhisigma2=-0.5/(_sigma*_sigma);
  /*Compute the kernel size so that the error in the first truncated
     coefficient is no larger than 0.5*KERNEL_WEIGHT.
    There is no point in going beyond this given our working precision.*/
  s=sqrt(0.5*M_PI)*_sigma*(1.0/KERNEL_WEIGHT);
  if(s>=1)len=0;
  else len=floor(_sigma*sqrt(-2*log(s)));
  kernel_len=len>=_max_len?_max_len-1:(int)len;
  kernel_sz=kernel_len<<1|1;
  kernel=(unsigned *)_ogg_malloc(kernel_sz*sizeof(*kernel));
  sum=0;
  for(ci=kernel_len;ci>0;ci--){
    kernel[kernel_len-ci]=kernel[kernel_len+ci]=
     (unsigned)(KERNEL_WEIGHT*scale*exp(nhisigma2*ci*ci)+0.5);
    sum+=kernel[kernel_len-ci];
  }
  kernel[kernel_len]=KERNEL_WEIGHT-(sum<<1);
  *_kernel=kernel;
  return kernel_sz;
}

typedef struct ssim_moments ssim_moments;

struct ssim_moments{
  unsigned mux;
  unsigned muy;
  unsigned x2;
  unsigned xy;
  unsigned y2;
  unsigned w;
};

#define SSIM_C1 (255*255*0.01*0.01)
#define SSIM_C2 (255*255*0.03*0.03)

static double calc_ssim(const unsigned char *_src,int _systride,
 const unsigned char *_dst,int _dystride,double _par,int _w,int _h){
  ssim_moments  *line_buf;
  ssim_moments **lines;
  double         ssim;
  double         ssimw;
  unsigned      *hkernel;
  int            hkernel_sz;
  int            hkernel_offs;
  unsigned      *vkernel;
  int            vkernel_sz;
  int            vkernel_offs;
  int            log_line_sz;
  int            line_sz;
  int            line_mask;
  int            x;
  int            y;
  vkernel_sz=gaussian_filter_init(&vkernel,_h*(1.5/256),_w<_h?_w:_h);
  vkernel_offs=vkernel_sz>>1;
  for(line_sz=1,log_line_sz=0;line_sz<vkernel_sz;line_sz<<=1,log_line_sz++);
  line_mask=line_sz-1;
  lines=(ssim_moments **)_ogg_malloc(line_sz*sizeof(*lines));
  lines[0]=line_buf=(ssim_moments *)_ogg_malloc(line_sz*_w*sizeof(*line_buf));
  for(y=1;y<line_sz;y++)lines[y]=lines[y-1]+_w;
  hkernel_sz=gaussian_filter_init(&hkernel,_h*(1.5/256)/_par,_w<_h?_w:_h);
  hkernel_offs=hkernel_sz>>1;
  ssim=0;
  ssimw=0;
  for(y=0;y<_h+vkernel_offs;y++){
    ssim_moments *buf;
    int           k;
    int           k_min;
    int           k_max;
    if(y<_h){
      buf=lines[y&line_mask];
      for(x=0;x<_w;x++){
        ssim_moments m;
        memset(&m,0,sizeof(m));
        k_min=hkernel_offs-x<=0?0:hkernel_offs-x;
        k_max=x+hkernel_offs-_w+1<=0?
         hkernel_sz:hkernel_sz-(x+hkernel_offs-_w+1);
        for(k=k_min;k<k_max;k++){
          unsigned s;
          unsigned d;
          unsigned w;
          s=_src[x-hkernel_offs+k];
          d=_dst[x-hkernel_offs+k];
          w=hkernel[k];
          m.mux+=w*s;
          m.muy+=w*d;
          m.x2+=w*s*s;
          m.xy+=w*s*d;
          m.y2+=w*d*d;
          m.w+=w;
        }
        *(buf+x)=*&m;
      }
      _src+=_systride;
      _dst+=_dystride;
    }
    if(y>=vkernel_offs){
      k_min=vkernel_sz-y-1<=0?0:vkernel_sz-y-1;
      k_max=y+1-_h<=0?vkernel_sz:vkernel_sz-(y+1-_h);
      for(x=0;x<_w;x++){
        ssim_moments m;
        double       c1;
        double       c2;
        double       mx2;
        double       mxy;
        double       my2;
        double       w;
        memset(&m,0,sizeof(m));
        for(k=k_min;k<k_max;k++){
          unsigned w;
          buf=lines[y+1-vkernel_sz+k&line_mask]+x;
          w=vkernel[k];
          m.mux+=w*buf->mux;
          m.muy+=w*buf->muy;
          m.x2+=w*buf->x2;
          m.xy+=w*buf->xy;
          m.y2+=w*buf->y2;
          m.w+=w*buf->w;
        }
        w=m.w;
        c1=SSIM_C1*w*w;
        c2=SSIM_C2*w*w;
        mx2=m.mux*(double)m.mux;
        mxy=m.mux*(double)m.muy;
        my2=m.muy*(double)m.muy;
        ssim+=m.w*(2*mxy+c1)*(c2+2*(m.xy*w-mxy))/
         ((mx2+my2+c1)*(m.x2*w-mx2+m.y2*w-my2+c2));
        ssimw+=m.w;
      }
    }
  }
  _ogg_free(line_buf);
  _ogg_free(lines);
  return ssim/ssimw;
}



static void usage(char *_argv[]){
  fprintf(stderr,"Usage: %s [options] <video1> <video2>\n"
   "    <video1> and <video2> may be either YUV4MPEG or Ogg Theora files.\n\n"
   "    Options:\n\n"
   "      -f --frame-type Show frame type and QI value for each Theora frame.\n"
   "      -r --raw        Show raw SSIM scores, instead of"
   " 10*log10(1/(1-ssim)).\n"
   "      -s --summary    Only output the summary line.\n"
   "      -y --luma-only  Only output values for the luma channel.\n",_argv[0]);
}

typedef double (*convert_ssim_func)(double _ssim,double _weight);

static double convert_ssim_raw(double _ssim,double _weight){
  return _ssim/_weight;
}

static double convert_ssim_db(double _ssim,double _weight){
  return 10*(log10(_weight)-log10(_weight-_ssim));
}

int main(int _argc,char *_argv[]){
  video_input        vid1;
  video_input_info   info1;
  video_input        vid2;
  video_input_info   info2;
  convert_ssim_func  convert;
  double             gssim[3];
  double             cweight;
  double             par;
  int                frameno;
  FILE              *fin;
  int                long_option_index;
  int                c;
#ifdef _WIN32
  /*We need to set stdin/stdout to binary mode on windows.
    Beware the evil ifdef.
    We avoid these where we can, but this one we cannot.
    Don't add any more, you'll probably go to hell if you do.*/
  _setmode(_fileno(stdin),_O_BINARY);
#endif
  /*Process option arguments.*/
  convert=convert_ssim_db;
  while((c=getopt_long(_argc,_argv,optstring,options,&long_option_index))!=EOF){
    switch(c){
      case 'f':show_frame_type=1;break;
      case 'r':convert=convert_ssim_raw;break;
      case 's':summary_only=1;break;
      case 'y':luma_only=1;break;
      default:{
        usage(_argv);
        exit(EXIT_FAILURE);
      }break;
    }
  }
  if(optind+2!=_argc){
    usage(_argv);
    exit(EXIT_FAILURE);
  }
  fin=strcmp(_argv[optind],"-")==0?stdin:fopen(_argv[optind],"rb");
  if(fin==NULL){
    fprintf(stderr,"Unable to open '%s' for extraction.\n",_argv[optind]);
    exit(EXIT_FAILURE);
  }
  fprintf(stderr,"Opening %s...\n",_argv[optind]);
  if(video_input_open(&vid1,fin)<0)exit(EXIT_FAILURE);
  video_input_get_info(&vid1,&info1);
  fin=strcmp(_argv[optind+1],"-")==0?stdin:fopen(_argv[optind+1],"rb");
  if(fin==NULL){
    fprintf(stderr,"Unable to open '%s' for extraction.\n",_argv[optind+1]);
    exit(EXIT_FAILURE);
  }
  fprintf(stderr,"Opening %s...\n",_argv[optind+1]);
  if(video_input_open(&vid2,fin)<0)exit(EXIT_FAILURE);
  video_input_get_info(&vid2,&info2);
  /*Check to make sure these videos are compatible.*/
  if(info1.pic_w!=info2.pic_w||info1.pic_h!=info2.pic_h){
    fprintf(stderr,"Video resolution does not match.\n");
    exit(EXIT_FAILURE);
  }
  if(info1.pixel_fmt!=info2.pixel_fmt){
    fprintf(stderr,"Pixel formats do not match.\n");
    exit(EXIT_FAILURE);
  }
  if((info1.pic_x&!(info1.pixel_fmt&1))!=(info2.pic_x&!(info2.pixel_fmt&1))||
   (info1.pic_y&!(info1.pixel_fmt&2))!=(info2.pic_y&!(info2.pixel_fmt&2))){
    fprintf(stderr,"Chroma subsampling offsets do not match.\n");
    exit(EXIT_FAILURE);
  }
  if(info1.fps_n*(ogg_int64_t)info2.fps_d!=
   info2.fps_n*(ogg_int64_t)info1.fps_d){
    fprintf(stderr,"Warning: framerates do not match.\n");
    fprintf(stderr,"info1.fps_n=%i info1.fps_d=%i info2.fps_n=%i info2.fps_d=%i\n",info1.fps_n,info1.fps_d,info2.fps_n,info2.fps_d);
  }
  if(info1.par_n*(ogg_int64_t)info2.par_d!=
   info2.par_n*(ogg_int64_t)info1.par_d){
    fprintf(stderr,"Warning: aspect ratios do not match.\n");
  }
  par=info1.par_n>0&&info2.par_d>0?
   info1.par_n/(double)info2.par_d:1;
  gssim[0]=gssim[1]=gssim[2]=0;
  /*We just use a simple weighting to get a single full-color score.
    In reality the CSF for chroma is not the same as luma.*/
  cweight=0.25*(4>>(!(info1.pixel_fmt&1)+!(info1.pixel_fmt&2)));
  for(frameno=0;;frameno++){
    video_input_ycbcr f1;
    video_input_ycbcr f2;
    double          ssim[3];
    char            tag1[5];
    char            tag2[5];
    int             ret1;
    int             ret2;
    int             pli;
    ret1=video_input_fetch_frame(&vid1,f1,tag1);
    ret2=video_input_fetch_frame(&vid2,f2,tag2);
    if(ret1==0&&ret2==0)break;
    else if(ret1<0||ret2<0)break;
    else if(ret1==0){
      fprintf(stderr,"%s ended before %s.\n",
       _argv[optind],_argv[optind+1]);
      break;
    }
    else if(ret2==0){
      fprintf(stderr,"%s ended before %s.\n",
       _argv[optind+1],_argv[optind]);
      break;
    }
    /*Okay, we got one frame from each.*/
    for(pli=0;pli<3;pli++){
      int xdec;
      int ydec;
      xdec=pli&&!(info1.pixel_fmt&1);
      ydec=pli&&!(info1.pixel_fmt&2);
      ssim[pli]=calc_ssim(
       f1[pli].data+(info1.pic_y>>ydec)*f1[pli].stride+(info1.pic_x>>xdec),
       f1[pli].stride,
       f2[pli].data+(info2.pic_y>>ydec)*f2[pli].stride+(info2.pic_x>>xdec),
       f2[pli].stride,
       par,((info1.pic_x+info1.pic_w+xdec)>>xdec)-(info1.pic_x>>xdec),
       ((info1.pic_y+info1.pic_h+ydec)>>ydec)-(info1.pic_y>>ydec));
      gssim[pli]+=ssim[pli];
    }
    if(!summary_only){
      if(show_frame_type)printf("%s%s",tag1,tag2);
      if(!luma_only){
        printf("%08i: %-8G  (Y': %-8G  Cb: %-8G  Cr: %-8G)\n",frameno,
         convert(ssim[0]+cweight*(ssim[1]+ssim[2]),1+2*cweight),
         convert(ssim[0],1),convert(ssim[1],1),convert(ssim[2],1));
      }
      else printf("%08i: %-8G\n",frameno,convert(ssim[0],1));
    }
  }
  if(!luma_only){
    printf("Total: %-8G  (Y': %-8G  Cb: %-8G  Cr: %-8G)\n",
     convert(gssim[0]+cweight*(gssim[1]+gssim[2]),(1+2*cweight)*frameno),
     convert(gssim[0],frameno),convert(gssim[1],frameno),
     convert(gssim[2],frameno));
  }
  else printf("Total: %-8G\n",convert(gssim[0],frameno));
  video_input_close(&vid1);
  video_input_close(&vid2);
  return EXIT_SUCCESS;
}
