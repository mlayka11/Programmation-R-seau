// awale_sdl2.c
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define W 960
#define H 540
#define PIT_R 35
#define ANIM_DELAY_MS 120

static int board[12];
static int p1 = 0, p2 = 0;
static int player = 1;      // 1 ou 2
static bool gameOver = false;

static const char *FONT_PATH_CANDIDATES[] = {
  "/System/Library/Fonts/Supplemental/Arial.ttf",  // ✅ chemin trouvé sur ton Mac
  "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
  "/Library/Fonts/Arial.ttf",
  "DejaVuSans.ttf",
  "Arial.ttf",
  NULL
};


void init_board(void){
  for(int i=0;i<12;i++) board[i]=4;
  p1=0; p2=0; player=1; gameOver=false;
}

typedef struct { int cx, cy; } PitPos;

void compute_positions(PitPos pos[12]){
  int marginX = 80, gapX = (W - 2*marginX) / 5;
  int yTop = H/2 - 80;
  int yBot = H/2 + 80;

  for(int i=0;i<6;i++){
    int idx = 11 - i; // mapping
    pos[idx].cx = marginX + i*gapX;
    pos[idx].cy = yTop;
  }

  for(int i=0;i<6;i++){
    pos[i].cx = marginX + i*gapX;
    pos[i].cy = yBot;
  }
}

bool point_in_circle(int x,int y,int cx,int cy,int r){
  int dx=x-cx, dy=y-cy; return dx*dx + dy*dy <= r*r;
}

void draw_text(SDL_Renderer *ren, TTF_Font *font, const char *txt, int x, int y, SDL_Color col, bool center){
  SDL_Surface *surf = TTF_RenderUTF8_Blended(font, txt, col);
  if(!surf) return;
  SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
  SDL_Rect dst = { x, y, surf->w, surf->h };
  if(center){ dst.x -= surf->w/2; dst.y -= surf->h/2; }
  SDL_FreeSurface(surf);
  SDL_RenderCopy(ren, tex, NULL, &dst);
  SDL_DestroyTexture(tex);
}

void fill_circle(SDL_Renderer *ren, int cx, int cy, int r){
  for(int dy=-r; dy<=r; dy++){
    int w = (int)sqrt((double)r*r - dy*dy);
    SDL_RenderDrawLine(ren, cx - w, cy + dy, cx + w, cy + dy);
  }
}

void draw_board(SDL_Renderer *ren, TTF_Font *font, PitPos pos[12]){
  SDL_SetRenderDrawColor(ren, 245, 245, 248, 255);
  SDL_RenderClear(ren);

  // Header
  char hdr[64];
  snprintf(hdr,sizeof(hdr),"Awalé — Joueur %d %s", player, gameOver ? "(terminé)" : "");
  draw_text(ren, font, hdr, W/2, 30, (SDL_Color){30,30,30,255}, true);

  // Scores
  char s1[32], s2[32];
  snprintf(s1,sizeof(s1),"J1: %d", p1); snprintf(s2,sizeof(s2),"J2: %d", p2);
  draw_text(ren, font, s1, 100, 70, (SDL_Color){20,20,20,255}, false);
  draw_text(ren, font, s2, W-160, 70, (SDL_Color){20,20,20,255}, false);

  // Board plate
  SDL_Rect plate = {60, H/2 - 130, W-120, 260};
  SDL_SetRenderDrawColor(ren, 230, 227, 214, 255); // bois clair
  SDL_RenderFillRect(ren, &plate);
  SDL_SetRenderDrawColor(ren, 200, 196, 182, 255);
  SDL_RenderDrawRect(ren, &plate);

  // Pits
  for(int i=0;i<12;i++){
    bool owner1 = (i>=0 && i<=5);
    bool owner2 = (i>=6 && i<=11);
    bool active = (!gameOver) && ((player==1 && owner1 && board[i]>0) || (player==2 && owner2 && board[i]>0));

    // Anneau
    SDL_SetRenderDrawColor(ren, owner1 ? 218 : 199, owner1 ? 180 : 199, owner1 ? 92 : 120, 255);
    fill_circle(ren, pos[i].cx, pos[i].cy, PIT_R+2);

    // Fond trou
    SDL_SetRenderDrawColor(ren, 252, 249, 240, 255);
    fill_circle(ren, pos[i].cx, pos[i].cy, PIT_R);

    // Surbrillance joueur actif
    if(active){
      SDL_SetRenderDrawColor(ren, 80, 140, 255, 255);
      for(int r=PIT_R+3;r<=PIT_R+6;r+=1) fill_circle(ren,pos[i].cx,pos[i].cy,r);
    }

    // Nombre de graines
    char nb[8]; snprintf(nb,sizeof(nb),"%d",board[i]);
    draw_text(ren, font, nb, pos[i].cx, pos[i].cy, (SDL_Color){30,30,30,255}, true);

    // Etiquette index côté
    char tag[12];
    snprintf(tag,sizeof(tag), owner1?"J1-%d": "J2-%d", owner1 ? (i+1) : (i-5));
    draw_text(ren, font, tag, pos[i].cx, pos[i].cy + PIT_R + 16, (SDL_Color){70,70,70,255}, true);
  }

  // Aide
  const char *help = gameOver ? "Appuyez sur R pour rejouer — ESC pour quitter"
                              : "Cliquez sur une case de votre côté (1–6) — ESC pour quitter";
  draw_text(ren, font, help, W/2, H-30, (SDL_Color){60,60,60,255}, true);

  SDL_RenderPresent(ren);
}

void animate_step(SDL_Renderer *ren, TTF_Font *font, PitPos pos[12], int pitIndex){
  (void)pitIndex; // visuellement on se contente de redraw rapide
  draw_board(ren, font, pos);
  SDL_Delay(ANIM_DELAY_MS);
}

// Effectue un coup complet selon la logique du code C fourni.
// Retourne le gain obtenu ce tour.
int play_move(SDL_Renderer *ren, TTF_Font *font, PitPos pos[12], int pickIndex){
  int gain = 0;

  // Validation côté joueur
  if(player==1 && !(pickIndex>=0 && pickIndex<=5)) return 0;
  if(player==2 && !(pickIndex>=6 && pickIndex<=11)) return 0;
  if(board[pickIndex]==0) return 0;

  // Seme
  int pit = pickIndex;   // ici 0-based direct
  int n = board[pit];
  board[pit] = 0;

  for(int i=0;i<n;i++){
    pit = (pit + 1) % 12;
    board[pit] += 1;
    animate_step(ren, font, pos, pit);
  }

  // Capture comme dans ton C :
  // - si la case finale vaut 2 ou 3 : capturer
  // - puis reculer n fois, capturer les cases valant 2 ou 3
  if(board[pit]==2 || board[pit]==3){
    gain += board[pit];
    board[pit] = 0;
    animate_step(ren, font, pos, pit);

    int pcur = pit;
    for(int i=0;i<n;i++){
      if(pcur<0) pcur = 11;
      if(board[pcur]==2 || board[pcur]==3){
        gain += board[pcur];
        board[pcur] = 0;
        animate_step(ren, font, pos, pcur);
      }
      pcur = (pcur - 1 + 12) % 12;
    }
  }

  return gain;
}

int main(void){
  if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)!=0){ fprintf(stderr,"SDL_Init: %s\n",SDL_GetError()); return 1; }
  if(TTF_Init()!=0){ fprintf(stderr,"TTF_Init: %s\n",TTF_GetError()); SDL_Quit(); return 1; }

  // Font loading
  TTF_Font *font=NULL;
  for(int i=0; FONT_PATH_CANDIDATES[i]; i++){
    font = TTF_OpenFont(FONT_PATH_CANDIDATES[i], 18);
    if(font){ printf("Using font: %s\n", FONT_PATH_CANDIDATES[i]); break; }
  }
  if(!font){
    fprintf(stderr,"Impossible de trouver une police (TTF). Placez DejaVuSans.ttf près de l'exécutable.\n");
    TTF_Quit(); SDL_Quit(); return 1;
  }

  SDL_Window *win = SDL_CreateWindow("Awalé – SDL2",
      SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, W, H, SDL_WINDOW_SHOWN);
  if(!win){ fprintf(stderr,"Window: %s\n",SDL_GetError()); TTF_CloseFont(font); TTF_Quit(); SDL_Quit(); return 1; }
  SDL_Renderer *ren = SDL_CreateRenderer(win,-1,SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC);
  if(!ren){ fprintf(stderr,"Renderer: %s\n",SDL_GetError()); SDL_DestroyWindow(win); TTF_CloseFont(font); TTF_Quit(); SDL_Quit(); return 1; }

  PitPos pos[12]; compute_positions(pos);
  init_board();

  bool running=true;
  while(running){
    draw_board(ren, font, pos);

    SDL_Event e;
    while(SDL_PollEvent(&e)){
      if(e.type==SDL_QUIT){ running=false; break; }
      if(e.type==SDL_KEYDOWN){
        if(e.key.keysym.sym==SDLK_ESCAPE){ running=false; break; }
        if(gameOver && e.key.keysym.sym==SDLK_r){ init_board(); }
      }
      if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT && !gameOver){
        int mx=e.button.x, my=e.button.y;

        // Chercher la case cliquée valide
        int pick=-1;
        for(int i=0;i<12;i++){
          if(point_in_circle(mx,my,pos[i].cx,pos[i].cy,PIT_R)){
            pick=i; break;
          }
        }
        if(pick==-1) continue;

        // Valider côté / non vide
        bool ok = ((player==1 && pick>=0 && pick<=5 && board[pick]>0) ||
                   (player==2 && pick>=6 && pick<=11 && board[pick]>0));
        if(!ok) continue;

        int gained = play_move(ren, font, pos, pick);
        if(player==1) p1 += gained; else p2 += gained;

        if(p1>=25 || p2>=25){
          gameOver = true;
        }else{
          player = (player==1)?2:1;
        }
      }
    }
  }

  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  TTF_CloseFont(font);
  TTF_Quit();
  SDL_Quit();
  return 0;
}
