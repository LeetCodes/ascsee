#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <wand/MagickWand.h>

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
/* (death) larva stage -> pupa -> maturity */
#define MAP " .',:;=*i!|lX@#"

unsigned char
  *g_life, 
  *g_pixel;

int 
  g_width, 
  g_height;

char* pLife(int x, int y) {
  return &g_life[x * g_width + y];
}
unsigned char Life(int x, int y) {
  return g_life[x * g_width + y];
}
char* pPixel(int x, int y) {
  return &g_pixel[x * g_width + y];
}
unsigned char Pixel(int x, int y) {
  return g_pixel[x * g_width + y];
}

#define ARG(x, y) (argc > x ? atoi(argv[x]) : y )
int main(int argc, char*argv[]) {
  fd_set fd_r;
  srand(time(0));
  struct timeval ts;

  MagickBooleanType stat;
  MagickWand *wand;

  int 
    cutoff = 225, 
    contrastRounds = rand() % 5,
    moveradius = 2,
    MATURE = 16,
    seedCount = rand() % 10 + 1,

    // chance of reproducing
    reproduce = ARG(2, rand() % 40 + 10), 

    // bigger means fewer children
    litterSize = ARG(3, rand() % 8 + 2),

    // chance of dying
    dieoff = ARG(4, rand() % 10 + 2),

    // chance of trying to move around
    move = ARG(5, rand() % 20 + 2), 

    // chance of growth
    growth = ARG(6, rand() % 40 + 2), 

    // How many rounds to do before displaying to the screen
    viewEvery = 15,
    tps = 1000;

  FILE *fdesc;

  int  
    fd,
    ix,
    iy,
    i,
    j,
    maturity,
    turn = 0,
    best, 
    bstIx, 
    bstIy, 
    limI, 
    limJ;

  if(argc > 1) {
    fd = open(argv[1], O_RDONLY);
    if(!fd) {
      printf("Can't open file: %s\n", argv[1]);
      exit(-1);
    }
  } else {
    printf("Please tell me the image file to use.\n");
    exit(-2);
  }

  ts.tv_sec = 0;
  // Determine the height and width of the users' current terminal
  struct winsize w;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

  g_height = w.ws_row - 2;
  g_width = w.ws_col;
  memset(g_life, g_width * g_height, 0);

  // Load the file
  MagickWandGenesis();

  wand = NewMagickWand();
  fdesc = fdopen(fd, "rb");
  stat = MagickReadImageFile(wand, fdesc);

  if (stat == MagickFalse) {
    printf("Image Magick can't read %s. It can usually read just about anything!\nIs this an image file? Are you sure?\n", argv[1]);
    return 0;
  }

  g_life = (unsigned char*)malloc(sizeof(unsigned char) * g_width * g_height);
  g_pixel = malloc(g_width * g_height);

  MagickResetIterator(wand);

  while (MagickNextImage(wand) != MagickFalse);

  for(ix = 0; ix < contrastRounds; ix++) {
    MagickContrastImage( wand, MagickTrue);
  }

  //MagickWriteImage(wand, "/tmp/file.png");

  MagickResizeImage(
    wand,
    g_width, g_height,
    LanczosFilter,
    1.0
  );
  MagickExportImagePixels(wand,
    0, 0,
    g_width, g_height,
    "I",
    CharPixel,
    g_pixel
  );

  // Seed the map with some values
  for(ix = 0; ix < seedCount; ix++) {
    g_life[rand() % (g_height * g_width)] = rand() % 100 + 16;
  }

  // We don't look at every step because that would be very tedious.
  for(;;turn++) {
    for(ix = 0; ix < g_height; ix++) {
      for(iy = 0; iy < g_width; iy++) {

        if(turn % viewEvery == 0) { 
          putchar(MAP[Life(ix,iy) >> 4]);
        }

        maturity = Life(ix, iy);
        if(maturity > 0) {

          // The chance of death is 
          //
          // random number % the brightness value of the pixel being < the dieoff
          // number.
          if(maturity > Pixel(ix,iy) && (rand() % (Pixel(ix,iy) + 1)) < dieoff ) { 
            g_life[g_width * ix + iy] = 0;
            // reproduces 
          } else if(maturity > MATURE && rand() % reproduce == 0) { 
              limI = MIN(ix + 2, g_height);
              limJ = MIN(iy + 2, g_width);

              for(i = MAX(ix - 2, 0); i < limI; i++){
                for(j = MAX(iy - 2, 0); j < limJ; j++){
                if(Life(i,j) == 0 && (rand() % litterSize == 0)) {
                  *pLife(i,j) = 1;
                }
              }
            }
            // move to a better spot 
          } else if(rand() % move == 0) { 

              // current position 
              best = Pixel(ix, iy); 
              bstIx = -1;
              limI = MIN(ix + moveradius, g_height);
              limJ = MIN(iy + moveradius, g_width);

              for(i = MAX(ix - moveradius, 0); i < limI; i++){
                for(j = MAX(iy - moveradius, 0); j < limJ; j++){
                  if(Life(i, j) == 0) {
                    if(Pixel(i, j) > best) {
                      bstIx = i;
                      bstIy = j;
                      best = Pixel(i, j);
                    }
                  }
                }
              }
              // found a new spot 
              if(bstIx != -1) {
                *pLife(bstIx, bstIy) =  Life(ix, iy);
                *pLife(ix, iy) = 0;
              }
            // moving is more expensive then staying 
          } else if(rand() % growth == 0) { 
            
            if(Life(ix, iy) < cutoff) {
              g_life[ix * g_width + iy]++; 
            }
          }
        }
      }
      if(turn % viewEvery == 0) {
        putchar('\n');
      }
    }

    // This makes sure we don't peg the processor.
    FD_ZERO(&fd_r);
    FD_SET(0, &fd_r);

    // This is our sleep value - at least 300
    ts.tv_usec = 1000000 / tps;
    select(1, &fd_r, NULL, NULL, &ts);
    if(FD_ISSET(0, &fd_r)) { 
      return(0);
    }

    // This returns everything to the top for another draw.
    if(turn % viewEvery == 0) {
      printf("reproduce:%d litter:%d die:%d move:%d grow:%d \033[100D\033[%dA", 
        reproduce,
        litterSize,
        dieoff,
        move,
        growth,
        g_height);
    }
  }
  free(g_life);
  free(g_pixel);
  return 0;
}
