#include "raylib.h"
#include "architecture.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

/* ── Layout ────────────────────────────────────────────────── */
#define WIN_W       1100
#define WIN_H        620
#define TB_H          34   /* title bar */
#define BB_H          46   /* bottom bar */
#define L_W          700   /* left panel */
#define R_W         (WIN_W - L_W)
#define CY          TB_H   /* content top-y */

/* pipeline boxes */
#define BX_W   118
#define BX_H    88
#define BX_GAP  13
#define BX_X0   11

/* ── Color palette (deep navy + neon) ─────────────────────── */
#define C_BG         (Color){ 7,  9, 18,255}
#define C_PANEL      (Color){12, 16, 30,255}
#define C_RAISED     (Color){18, 26, 50,255}
#define C_BD_LO      (Color){28, 42, 80,255}
#define C_BD_MID     (Color){46, 70,128,255}

#define C_TXT_HI     (Color){215,232,255,255}
#define C_TXT_MID    (Color){142,172,218,255}
#define C_TXT_LO     (Color){ 72,100,152,255}
#define C_TXT_MIN    (Color){ 38, 56, 96,255}

/* stage: active (cyan) */
#define C_CYN_BG     (Color){  0, 82,120,255}
#define C_CYN_BD     (Color){  0,210,252,255}
#define C_CYN_TX     (Color){ 90,232,255,255}

/* stage: fetch (green) */
#define C_GRN_BG     (Color){  0, 68, 34,255}
#define C_GRN_BD     (Color){ 38,196, 88,255}
#define C_GRN_TX     (Color){ 86,228,118,255}

/* stage: flush (red) */
#define C_RED_BG     (Color){ 84, 12, 18,255}
#define C_RED_BD     (Color){216, 46, 60,255}
#define C_RED_TX     (Color){255, 96,110,255}

/* stage: stall (amber) */
#define C_AMB_BG     (Color){ 70, 50,  0,255}
#define C_AMB_BD     (Color){218,160,  0,255}
#define C_AMB_TX     (Color){252,192, 48,255}

/* stage: empty */
#define C_EMP_BG     (Color){ 12, 16, 30,255}
#define C_EMP_BD     (Color){ 26, 40, 74,255}
#define C_EMP_TX     (Color){ 44, 64,104,255}

/* semantic accents */
#define C_GREEN      (Color){ 46,202, 90,255}
#define C_PURPLE     (Color){178,136,255,255}
#define C_BLUE       (Color){ 80,164,255,255}
#define C_RED        (Color){255, 96,110,255}
#define C_AMBER      (Color){226,178, 50,255}
#define C_CYAN       (Color){  0,210,252,255}

/* ── State machine ─────────────────────────────────────────── */
typedef enum {
    SIM_IDLE, SIM_READY, SIM_PAUSED, SIM_RUNNING, SIM_DONE, SIM_CAPLIMIT
} SimState;

/* ── Snapshots ─────────────────────────────────────────────── */
#define MAX_CYCLES 512

typedef struct { Processor cpu; } Snap;

static Snap     snaps[MAX_CYCLES];
static int      nsnaps       = 0;
static int      cur          = 0;
static SimState state        = SIM_IDLE;
static Processor cpu;
static char     loaded[260]  = "";
static char     errmsg[128]  = "";
static bool     scroll_end   = false;

static const int SPD_FRAMES[] = {120, 60, 30, 12};
static int  spd_idx    = 1;
static int  play_frame = 0;
static int  log_scroll = 0;

/* ── Font ──────────────────────────────────────────────────── */
static Font G;
static bool G_ok = false;

static void font_init(void) {
    const char *paths[] = {
        "C:/Windows/Fonts/CascadiaCode.ttf",
        "C:/Windows/Fonts/CascadiaMono.ttf",
        "C:/Windows/Fonts/consola.ttf",
        NULL
    };
    for (int i = 0; paths[i]; i++) {
        G = LoadFontEx(paths[i], 32, NULL, 0);
        if (IsFontReady(G)) { G_ok = true; return; }
    }
}

/* draw helpers */
static void T(const char *s, int x, int y, int sz, Color c) {
    if (G_ok) DrawTextEx(G, s, (Vector2){(float)x,(float)y}, (float)sz, 0.55f, c);
    else       DrawText(s, x, y, sz, c);
}
static int TW(const char *s, int sz) {
    if (G_ok) return (int)MeasureTextEx(G, s, (float)sz, 0.55f).x;
    return MeasureText(s, sz);
}
static void TC(const char *s, int rx, int ry, int rw, int rh, int sz, Color c) {
    T(s, rx + rw/2 - TW(s,sz)/2, ry + rh/2 - sz/2, sz, c);
}

/* rounded box */
static void rbox(int x, int y, int w, int h, float r, Color bg, Color bd, float bthick) {
    Rectangle rc = {(float)x,(float)y,(float)w,(float)h};
    DrawRectangleRounded(rc, r, 6, bg);
    DrawRectangleRoundedLines(rc, r, 6, bthick, bd);
}

/* rounded box with outer glow */
static void rbox_glow(int x, int y, int w, int h, Color bg, Color bd) {
    DrawRectangleRoundedLines((Rectangle){x-3.f,y-3.f,w+6.f,h+6.f}, 0.14f,6,1.f,
        (Color){bd.r,bd.g,bd.b,20});
    DrawRectangleRoundedLines((Rectangle){x-1.f,y-1.f,w+2.f,h+2.f}, 0.13f,6,1.f,
        (Color){bd.r,bd.g,bd.b,65});
    rbox(x,y,w,h,0.12f,bg,bd,1.5f);
}

/* corner bracket marks — the "targeting reticle" detail on active stages */
static void corner_marks(int x, int y, int w, int h, Color c, int s) {
    DrawLine(x,   y+s, x,   y,   c); DrawLine(x,   y,   x+s, y,   c);
    DrawLine(x+w-s,y, x+w, y,   c); DrawLine(x+w, y,   x+w, y+s, c);
    DrawLine(x,   y+h-s,x, y+h, c); DrawLine(x,   y+h, x+s, y+h, c);
    DrawLine(x+w-s,y+h,x+w,y+h,c); DrawLine(x+w,y+h,  x+w,y+h-s, c);
}

/* section header — left-panel (full-width) style */
static void section_full(int x, int y, int w, const char *lbl, Color accent, const char *tag) {
    DrawRectangle(x, y, w, 22, (Color){accent.r/10,accent.g/10,accent.b/10,255});
    DrawRectangle(x, y, w, 1,  (Color){accent.r,accent.g,accent.b,60});
    DrawRectangle(x, y+21, w, 1, (Color){accent.r,accent.g,accent.b,40});
    DrawRectangle(x, y, 2, 22, accent);
    T(lbl, x+10, y+6, 9, (Color){accent.r,accent.g,accent.b,200});
    if (tag && tag[0]) {
        int tw = TW(tag, 9);
        T(tag, x+w-tw-10, y+6, 9, (Color){accent.r,accent.g,accent.b,120});
    }
}

/* section header — right-panel (sidebar) style: just label + thin rule */
static void section_side(int x, int y, int w, const char *lbl, Color accent) {
    T(lbl, x+6, y+5, 8, (Color){accent.r,accent.g,accent.b,120});
    DrawLine(x, y+20, x+w, y+20, (Color){accent.r/6,accent.g/6,accent.b/6,255});
    DrawLine(x, y+21, x+w, y+21, (Color){accent.r/12,accent.g/12,accent.b/12,255});
}

/* button */
typedef struct { int x,y,w,h; } Btn;

static bool bhot(Btn b) {
    return CheckCollisionPointRec(GetMousePosition(),(Rectangle){b.x,b.y,b.w,b.h});
}
static bool bclk(Btn b) {
    return IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && bhot(b);
}
static void bdraw(Btn b, const char *lbl, Color bg, Color bd, Color tx, bool on) {
    Color fbg = on ? C_CYAN : (bhot(b)?(Color){bg.r+20,bg.g+20,bg.b+20,255}:bg);
    Color fbd = on ? C_CYAN : bd;
    Color ftx = on ? (Color){7,9,18,255} : tx;
    rbox(b.x,b.y,b.w,b.h,0.28f,fbg,fbd,1.2f);
    TC(lbl,b.x,b.y,b.w,b.h,11,ftx);
}

/* ── Forward declarations ──────────────────────────────────── */
static void do_advance(void);
static void do_back(void);
static void do_reset(void);
static void do_load(const char *path);
static void draw_titlebar(void);
static void draw_pipeline(void);
static void draw_fwd_banner(int y);
static void draw_log(int y, int h);
static void draw_regs(void);
static void draw_mem(void);
static void draw_botbar(void);

/* ── Simulation logic ──────────────────────────────────────── */

static void do_load(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot || (strcmp(dot,".txt")!=0 && strcmp(dot,".asm")!=0)) {
        snprintf(errmsg, sizeof(errmsg),
                 "Bad file type '%s' — drop a .txt or .asm file", dot ? dot : "(none)");
        return;
    }
    errmsg[0] = '\0';

    initialize_processor(&cpu);
    load_program_from_file(&cpu, path);

    snaps[0].cpu = cpu;
    nsnaps = 1; cur = 0; log_scroll = 0; play_frame = 0;

    const char *sl = strrchr(path,'\\');
    if (!sl) sl = strrchr(path,'/');
    strncpy(loaded, sl ? sl+1 : path, sizeof(loaded)-1);
    loaded[sizeof(loaded)-1] = '\0';

    state = SIM_READY;
}

static void do_advance(void) {
    if (state==SIM_DONE || state==SIM_IDLE || state==SIM_CAPLIMIT) return;

    scroll_end = true;
    if (cur+1 < nsnaps) {
        cpu = snaps[++cur].cpu;
    } else {
        if (nsnaps >= MAX_CYCLES) { state = SIM_CAPLIMIT; return; }
        pipeline_cycle(&cpu);
        snaps[nsnaps].cpu = cpu;
        cur = nsnaps++;
    }
    state = pipeline_empty(&cpu) ? SIM_DONE : SIM_PAUSED;
}

static void do_back(void) {
    if (cur <= 0) return;
    cpu = snaps[--cur].cpu;
    scroll_end = false;
    if (state == SIM_DONE || state == SIM_CAPLIMIT) state = SIM_PAUSED;
}

static void do_reset(void) {
    if (nsnaps == 0) return;
    cur = 0; cpu = snaps[0].cpu;
    play_frame = 0; log_scroll = 0; scroll_end = false;
    state = SIM_READY;
}

/* ── Draw ──────────────────────────────────────────────────── */

static void draw_titlebar(void) {
    DrawRectangle(0,0,WIN_W,TB_H,C_PANEL);
    DrawRectangle(0,TB_H-1,WIN_W,1,C_BD_LO);
    DrawRectangle(0,0,2,TB_H,C_CYAN);

    /* logo — small caps + subtitle */
    T("FILLET-O-NEUMANN", 14, 5, 13, C_TXT_HI);
    int tw = TW("FILLET-O-NEUMANN",13);
    T("· 5-stage pipeline simulator", 14+tw+4, 7, 10, C_TXT_MIN);

    if (state != SIM_IDLE) {
        /* right side: stacked CLK label + large cycle number */
        char cyc[12]; snprintf(cyc, sizeof(cyc), "%d", cur);
        int nw = TW(cyc, 18);
        int lw = TW("CLK",8);
        int bw = (nw>lw?nw:lw)+22;
        int bx = WIN_W-bw-8;
        DrawRectangle(bx-8, 0, 1, TB_H, C_BD_LO);
        T("CLK", bx+bw/2-lw/2, 3, 8, C_TXT_LO);
        T(cyc,  bx+bw/2-nw/2, 13, 18, C_CYAN);

        /* PC value */
        char pcstr[20]; snprintf(pcstr,sizeof(pcstr),"PC:%d",(int)snaps[cur].cpu.PC);
        int pw = TW(pcstr,9);
        DrawRectangle(bx-8-pw-20, 0, 1, TB_H, C_BD_LO);
        T("PC", bx-8-pw-20+6, 3, 8, C_TXT_LO);
        char pcv[12]; snprintf(pcv,sizeof(pcv),"%d",(int)snaps[cur].cpu.PC);
        int pvw = TW(pcv,18);
        T(pcv, bx-8-pvw-8, 13, 18, C_TXT_MID);
    }
}

static void draw_pipeline(void) {
    const Processor *p = &snaps[cur].cpu;

    const char *st_tag =
        state==SIM_PAUSED  ? "PAUSED"  :
        state==SIM_RUNNING ? "RUNNING" :
        state==SIM_DONE    ? "DONE"    :
        state==SIM_CAPLIMIT? "LIMIT"   : "READY";
    section_full(0,CY,L_W,"PIPELINE  STAGES",C_CYAN, st_tag);
    int by = CY + 22 + 10;

    typedef struct { const char *name; const PipelineReg *lr; int is_if; } SD;
    SD s[5] = {
        {"IF",  NULL,       1},
        {"ID",  &p->IF_ID,  0},
        {"EX",  &p->ID_EX,  0},
        {"MEM", &p->EX_MEM, 0},
        {"WB",  &p->MEM_WB, 0},
    };

    /* bus line */
    int bus_y = by + BX_H/2;
    DrawLine(BX_X0+BX_W, bus_y, BX_X0+4*(BX_W+BX_GAP), bus_y, C_BD_LO);

    for (int i=0; i<5; i++) {
        int x = BX_X0 + i*(BX_W+BX_GAP);
        Color bg,bd,tx; bool glow=false;
        const PipelineReg *lr = s[i].lr;

        if (s[i].is_if) {
            if (!p->fetching_done) { bg=C_GRN_BG; bd=C_GRN_BD; tx=C_GRN_TX; glow=true; }
            else                   { bg=C_EMP_BG; bd=C_EMP_BD; tx=C_EMP_TX; }
        } else if (lr && lr->valid) {
            if      (p->stall && i==1)                    { bg=C_AMB_BG; bd=C_AMB_BD; tx=C_AMB_TX; glow=true; }
            else if (p->branch_taken && (i==1||i==2))     { bg=C_RED_BG; bd=C_RED_BD; tx=C_RED_TX; glow=true; }
            else                                          { bg=C_CYN_BG; bd=C_CYN_BD; tx=C_CYN_TX; glow=true; }
        } else { bg=C_EMP_BG; bd=C_EMP_BD; tx=C_EMP_TX; }

        if (glow) {
            rbox_glow(x,by,BX_W,BX_H,bg,bd);
            corner_marks(x-2,by-2,BX_W+4,BX_H+4,(Color){bd.r,bd.g,bd.b,180},8);
        } else {
            rbox(x,by,BX_W,BX_H,0.12f,bg,bd,1.0f);
        }

        /* stage name */
        TC(s[i].name, x, by+4, BX_W, 20, 14, tx);

        /* inner separator */
        DrawLine(x+8, by+26, x+BX_W-8, by+26, (Color){bd.r,bd.g,bd.b,50});

        /* content */
        if (lr && lr->valid) {
            TC(lr->mnemonic, x, by+28, BX_W, 20, 13, C_TXT_HI);
            char det[28]="";
            if (i==1) snprintf(det,sizeof(det),"R%d  R%d",lr->r1,lr->r2);
            else if (i==2) snprintf(det,sizeof(det),"= %d",(int)lr->alu_result);
            else if (i==3 && lr->dest_reg>0)
                snprintf(det,sizeof(det),"R%d <- %d",lr->dest_reg,(int)lr->alu_result);
            else if (i==4 && lr->dest_reg>0)
                snprintf(det,sizeof(det),"R%d = %d",lr->dest_reg,(int)p->reg[lr->dest_reg]);
            if (det[0]) TC(det,x,by+52,BX_W,20,11,C_TXT_MID);
        } else if (s[i].is_if && !p->fetching_done) {
            char pb[16]; snprintf(pb,sizeof(pb),"PC = %d",(int)p->PC);
            TC(pb,x,by+38,BX_W,20,11,C_TXT_LO);
        } else {
            TC("—",x,by+38,BX_W,20,12,C_EMP_TX);
        }

        /* arrowhead (CCW in screen-Y-down) */
        if (i<4) {
            int ax = x+BX_W+BX_GAP/2;
            DrawTriangle(
                (Vector2){ax+5,  bus_y   },   /* tip    */
                (Vector2){ax-4,  bus_y-5 },   /* top-L  */
                (Vector2){ax-4,  bus_y+5 },   /* bot-L  */
                C_BD_MID
            );
            /* pipeline register label below bus */
            const char *reglbl[4]={"IF/ID","ID/EX","EX/MEM","MEM/WB"};
            int rlw = TW(reglbl[i], 8);
            T(reglbl[i], ax-rlw/2, bus_y+9, 8, C_TXT_MIN);
        }
    }

    /* done / caplimit badge */
    if (state==SIM_DONE || state==SIM_CAPLIMIT) {
        const char *msg = state==SIM_DONE ? "Simulation complete" : "Cycle limit reached (512)";
        Color mc = state==SIM_DONE ? C_GRN_BD : C_AMB_BD;
        Color mt = state==SIM_DONE ? C_GRN_TX : C_AMB_TX;
        Color mb = state==SIM_DONE ? (Color){0,68,34,255} : (Color){70,50,0,255};
        int mw = TW(msg,12)+22, mx = L_W/2-mw/2;
        int my = by+BX_H+8;
        DrawRectangleRounded((Rectangle){mx,my,mw,22},0.45f,6,mb);
        DrawRectangleRoundedLines((Rectangle){mx,my,mw,22},0.45f,6,1.0f,mc);
        T(msg,mx+11,my+5,12,mt);
    }
}

static void draw_fwd_banner(int y) {
    const Processor *p = &snaps[cur].cpu;
    if (!p->ID_EX.valid) return;

    int r1=p->ID_EX.r1, r2=p->ID_EX.r2;
    char fwd[80]="";

    if (p->EX_MEM.valid && p->EX_MEM.dest_reg>0 && p->EX_MEM.opcode!=OP_MOVR) {
        if ((r1>0&&r1==p->EX_MEM.dest_reg)||(r2>0&&r2==p->EX_MEM.dest_reg))
            snprintf(fwd,sizeof(fwd),"FWD  EX -> ID    R%d = %d",
                     p->EX_MEM.dest_reg,(int)p->EX_MEM.alu_result);
    }
    if (!fwd[0] && p->MEM_WB.valid && p->MEM_WB.dest_reg>0) {
        if ((r1>0&&r1==p->MEM_WB.dest_reg)||(r2>0&&r2==p->MEM_WB.dest_reg)) {
            int32_t v=(p->MEM_WB.opcode==OP_MOVR)?p->MEM_WB.mem_result:p->MEM_WB.alu_result;
            snprintf(fwd,sizeof(fwd),"FWD  MEM -> ID    R%d = %d",p->MEM_WB.dest_reg,(int)v);
        }
    }
    if (!fwd[0]) return;

    int fw = TW(fwd,11)+26;
    DrawRectangle(10,y+3,3,18,C_BLUE);
    DrawRectangle(13,y+3,fw,18,(Color){12,32,82,255});
    DrawRectangleLines(13,y+3,fw,18,(Color){46,70,128,255});
    T(fwd,22,y+7,11,C_BLUE);
}

static void draw_log(int y, int h) {
    section_full(0,y,L_W,"EVENT  LOG",C_BLUE, NULL);
    y+=22; h-=22;

    int lh=16, pad=13, vis=h/lh;

    typedef struct { char text[128]; Color col; bool hdr; } LL;
    static LL lines[MAX_CYCLES*7];
    int total=0;

    for (int c=1; c<=cur && c<nsnaps && total<MAX_CYCLES*7-12; c++) {
        const Processor *cv=&snaps[c].cpu, *pv=&snaps[c-1].cpu;

        /* cycle header chip */
        snprintf(lines[total].text,128,"%d",c);
        lines[total].col=C_CYAN; lines[total].hdr=true; total++;

        const PipelineReg *lr[4]={&cv->IF_ID,&cv->ID_EX,&cv->EX_MEM,&cv->MEM_WB};
        const char *sn[4]={"ID","EX","MEM","WB"};
        for (int s=0;s<4;s++) {
            if (!lr[s]->valid) continue;
            snprintf(lines[total].text,128,"%s  %s",sn[s],lr[s]->mnemonic);
            lines[total].col=C_TXT_LO; lines[total].hdr=false; total++;
        }

        for (int r=1;r<NUM_REGISTERS;r++) {
            if (cv->reg[r]==pv->reg[r]) continue;
            snprintf(lines[total].text,128,"WB  R%d = %d",r,(int)cv->reg[r]);
            lines[total].col=C_GREEN; lines[total].hdr=false; total++;
        }

        for (int a=DATA_MEM_START;a<MEMORY_SIZE;a++) {
            if (cv->memory[a]==pv->memory[a]) continue;
            snprintf(lines[total].text,128,"MEM[%d] = %d",a,(int)cv->memory[a]);
            lines[total].col=C_PURPLE; lines[total].hdr=false; total++;
        }

        if (cv->branch_taken) {
            snprintf(lines[total].text,128,"branch taken  –  flushed IF/ID");
            lines[total].col=C_RED; lines[total].hdr=false; total++;
        }
        if (cv->stall) {
            snprintf(lines[total].text,128,"stall inserted");
            lines[total].col=C_AMBER; lines[total].hdr=false; total++;
        }
    }

    int max_scroll = total>vis ? total-vis : 0;
    if (log_scroll>max_scroll) log_scroll=max_scroll;

    float wheel=GetMouseWheelMove();
    if (wheel!=0 && GetMouseX()<L_W && GetMouseY()>y) {
        scroll_end=false;
        log_scroll -= (int)wheel*2;
        if (log_scroll<0) log_scroll=0;
        if (log_scroll>max_scroll) log_scroll=max_scroll;
    }

    if (scroll_end) log_scroll=max_scroll;

    BeginScissorMode(0,y,L_W,h);
    for (int i=log_scroll; i<total && i-log_scroll<vis; i++) {
        int ly=y+(i-log_scroll)*lh;
        LL *l=&lines[i];
        if (l->hdr) {
            /* cycle pill */
            char chip[132]; snprintf(chip,sizeof(chip)," %s ",l->text);
            int cw=TW(chip,10)+4;
            DrawRectangleRounded((Rectangle){8,ly+2,cw,12},0.5f,4,(Color){0,210,252,22});
            DrawRectangleRoundedLines((Rectangle){8,ly+2,cw,12},0.5f,4,1.0f,(Color){0,210,252,90});
            T(chip,8,ly+3,10,C_CYAN);
        } else {
            DrawRectangle(6,ly+3,2,lh-6,l->col);
            T(l->text,pad,ly+3,11,l->col);
        }
    }
    EndScissorMode();
}

static void draw_regs(void) {
    const Processor *cv=&snaps[cur].cpu;
    const Processor *pv=cur>0?&snaps[cur-1].cpu:NULL;

    int rx=L_W, ry=CY;
    section_side(rx,ry,R_W,"REGISTERS",(Color){80,164,255,255});
    ry+=22;

    int cw=R_W/2, rh=16, pad=7;
    for (int i=0;i<NUM_REGISTERS;i++) {
        int col=i%2, row=i/2;
        int cx=rx+col*cw, cy=ry+row*rh;
        bool ch=pv&&(cv->reg[i]!=pv->reg[i]);
        bool nz=cv->reg[i]!=0;

        if (ch) {
            DrawRectangle(cx,cy,cw,rh,(Color){0,68,34,170});
            DrawRectangle(cx,cy,2,rh,C_GREEN);
        }

        /* group divider every 8 registers (at row 4, 8, 12) */
        if (col==0 && row>0 && (i%8)==0)
            DrawLine(rx, cy, rx+R_W, cy, C_BD_LO);

        char nm[8],vl[14];
        snprintf(nm,sizeof(nm),"R%d",i);
        snprintf(vl,sizeof(vl),"%d",(int)cv->reg[i]);
        Color lc = ch ? C_GREEN : (nz ? C_TXT_LO : C_TXT_MIN);
        Color vc = ch ? C_GREEN : (nz ? C_TXT_HI  : C_TXT_MIN);
        T(nm,cx+pad,cy+3,10,lc);
        T(vl,cx+cw-pad-TW(vl,10),cy+3,10,vc);
    }

    /* PC */
    int pc_y=ry+(NUM_REGISTERS/2)*rh;
    DrawRectangle(rx,pc_y,R_W,rh,(Color){10,28,68,255});
    DrawRectangle(rx,pc_y,2,rh,C_BLUE);
    char pcv[14]; snprintf(pcv,sizeof(pcv),"%d",(int)cv->PC);
    T("PC",rx+pad,pc_y+3,10,C_BLUE);
    T(pcv,rx+R_W-pad-TW(pcv,10),pc_y+3,10,C_BLUE);

    bool has_note=(state==SIM_DONE||state==SIM_CAPLIMIT);
    if (has_note) {
        int ny=pc_y+rh+4;
        const char *note=state==SIM_DONE?"Final state":"Truncated";
        Color nc=state==SIM_DONE?C_GREEN:C_AMBER;
        DrawRectangle(rx,ny,2,14,nc);
        T(note,rx+pad,ny+1,10,nc);
    }

    DrawLine(rx,pc_y+rh+(has_note?18:0),rx+R_W,pc_y+rh+(has_note?18:0),C_BD_LO);
}

static void draw_mem(void) {
    const Processor *cv=&snaps[cur].cpu;
    const Processor *pv=cur>0?&snaps[cur-1].cpu:NULL;

    bool has_note=(state==SIM_DONE||state==SIM_CAPLIMIT);
    int regs_h=22+(NUM_REGISTERS/2+1)*16+(has_note?18:0)+4;
    int rx=L_W, ry=CY+regs_h;
    int rh=WIN_H-BB_H-ry;

    section_side(rx,ry,R_W,"DATA MEMORY",C_PURPLE);
    ry+=22;

    int row_h=16, pad=7, rows=(rh-22)/row_h, drawn=0;
    for (int a=DATA_MEM_START;a<MEMORY_SIZE&&drawn<rows;a++) {
        if (cv->memory[a]==0) continue;
        int cy=ry+drawn*row_h;
        bool ch=pv&&(cv->memory[a]!=pv->memory[a]);
        if (ch) {
            DrawRectangle(rx,cy,R_W,row_h,(Color){36,8,66,200});
            DrawRectangle(rx,cy,2,row_h,C_PURPLE);
        }
        char as[12],vs[14];
        snprintf(as,sizeof(as),"%d:",a);
        snprintf(vs,sizeof(vs),"%d",(int)cv->memory[a]);
        T(as,rx+pad,cy+3,10,C_TXT_LO);
        T(vs,rx+R_W-pad-TW(vs,10),cy+3,10,ch?C_PURPLE:C_TXT_MID);
        drawn++;
    }
    if (!drawn) T("(all zeros)",rx+pad,ry+6,10,(Color){36,52,90,255});
}

static void draw_botbar(void) {
    int by=WIN_H-BB_H;
    DrawRectangle(0,by,WIN_W,BB_H,C_PANEL);
    DrawLine(0,by,WIN_W,by,C_BD_LO);

    int y=by+9;

    /* drop zone / error */
    if (errmsg[0]) {
        DrawRectangleRounded((Rectangle){10,y,240,28},0.18f,6,(Color){84,12,18,220});
        DrawRectangleRoundedLines((Rectangle){10,y,240,28},0.18f,6,1.0f,C_RED_BD);
        T(errmsg,18,y+8,10,C_RED_TX);
    } else {
        const char *dz=loaded[0]?loaded:"Drop .txt / .asm here";
        Color dzbd=loaded[0]?C_GRN_BD:C_BD_LO;
        Color dztx=loaded[0]?C_GRN_TX:C_TXT_LO;
        DrawRectangleRoundedLines((Rectangle){10,y,170,28},0.2f,6,1.0f,dzbd);
        T(dz,18,y+8,10,dztx);
    }

    /* dismiss error on click */
    if (errmsg[0] && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) errmsg[0]='\0';

    Color cnorm=(Color){18,26,50,255};
    Color cbnorm=C_BD_LO;
    Color ctnorm=C_TXT_MID;
    Color cgbg=(Color){32,128,52,255};
    Color cgbd=C_GRN_BD;
    Color cbbg=(Color){18,56,136,255};
    Color cbbd=C_BLUE;
    Color cwh=C_TXT_HI;

    int bx=188;

    Btn bback={bx,y,50,28};
    if (bclk(bback)&&state!=SIM_IDLE&&state!=SIM_READY) do_back();
    bdraw(bback,"< Back",cnorm,cbnorm,ctnorm,false);
    bx+=56;

    Btn bstep={bx,y,58,28};
    if (bclk(bstep)&&(state==SIM_READY||state==SIM_PAUSED)) do_advance();
    bdraw(bstep,"Step >",cgbg,cgbd,cwh,false);
    bx+=64;

    const char *pplbl=(state==SIM_RUNNING)?"|| Pause":">> Play";
    Btn bplay={bx,y,76,28};
    if (bclk(bplay)&&state!=SIM_IDLE&&state!=SIM_CAPLIMIT) {
        if (state==SIM_RUNNING)      state=SIM_PAUSED;
        else if (state==SIM_PAUSED||state==SIM_READY) {
            state=SIM_RUNNING; play_frame=0;
        }
    }
    bdraw(bplay,pplbl,cbbg,cbbd,cwh,state==SIM_RUNNING);
    bx+=82;

    Btn brst={bx,y,54,28};
    if (bclk(brst)&&state!=SIM_IDLE) do_reset();
    bdraw(brst,"Reset",cnorm,cbnorm,ctnorm,false);
    bx+=62;

    T("Speed",bx,y+9,10,C_TXT_LO);
    bx+=50;

    const char *slbls[]={"1/2x","1x","2x","5x"};
    for (int i=0;i<4;i++) {
        Btn bs={bx,y,40,28};
        if (bclk(bs)) spd_idx=i;
        bdraw(bs,slbls[i],cnorm,cbnorm,ctnorm,i==spd_idx);
        bx+=45;
    }

    /* state + progress (right-aligned) */
    const char *st_s=
        state==SIM_IDLE    ?"Waiting for file" :
        state==SIM_READY   ?"Ready" :
        state==SIM_PAUSED  ?"Paused" :
        state==SIM_RUNNING ?"Running" :
        state==SIM_DONE    ?"Complete" : "Limit reached";

    char prog[64];
    if (state==SIM_IDLE)
        snprintf(prog,sizeof(prog),"%s",st_s);
    else
        snprintf(prog,sizeof(prog),"%s  |  Cycle %d",st_s,cur);

    Color pc=
        state==SIM_DONE    ?C_GREEN  :
        state==SIM_CAPLIMIT?C_AMBER  :
        state==SIM_RUNNING ?C_CYAN   : C_TXT_LO;

    T(prog,WIN_W-TW(prog,11)-14,y+9,11,pc);
}

/* ── Main ──────────────────────────────────────────────────── */

int main(void) {
    InitWindow(WIN_W,WIN_H,"Fillet-O-Neumann Pipeline Simulator");
    SetTargetFPS(60);
    font_init();

    while (!WindowShouldClose()) {
        /* file drop */
        if (IsFileDropped()) {
            FilePathList dr=LoadDroppedFiles();
            if (dr.count>0) do_load(dr.paths[0]);
            UnloadDroppedFiles(dr);
        }

        /* keyboard */
        if (state!=SIM_IDLE) {
            if (IsKeyPressed(KEY_SPACE))  do_advance();
            if (IsKeyPressed(KEY_LEFT))   do_back();
            if (IsKeyPressed(KEY_R))      do_reset();
            if (IsKeyPressed(KEY_P)) {
                if (state==SIM_RUNNING) state=SIM_PAUSED;
                else if (state==SIM_PAUSED||state==SIM_READY) {
                    state=SIM_RUNNING; play_frame=0;
                }
            }
        }

        /* auto-play */
        if (state==SIM_RUNNING) {
            if (++play_frame>=SPD_FRAMES[spd_idx]) {
                play_frame=0; do_advance();
            }
        }

        /* render */
        BeginDrawing();
        ClearBackground(C_BG);

        /* subtle dot grid — makes the background feel designed, not default */
        for (int gx=0; gx<WIN_W; gx+=22)
            for (int gy=0; gy<WIN_H; gy+=22)
                DrawRectangle(gx, gy, 1, 1, (Color){32,52,96,30});

        draw_titlebar();
        DrawLine(0,CY,WIN_W,CY,C_BD_LO);
        DrawLine(L_W,CY,L_W,WIN_H-BB_H,C_BD_LO);

        if (state!=SIM_IDLE) {
            draw_pipeline();

            /* log starts below pipeline boxes + optional badge row */
            int pip_bot=CY+22+10+BX_H+(state==SIM_DONE||state==SIM_CAPLIMIT?36:8);
            draw_fwd_banner(pip_bot);
            draw_log(pip_bot+26, WIN_H-BB_H-(pip_bot+26));

            draw_regs();
            draw_mem();
        } else {
            /* idle drop panel */
            int cx=L_W/2, cy2=WIN_H/2;
            int bw=420, bh=140, bx=cx-bw/2, by2=cy2-bh/2;
            DrawRectangle(bx,by2,bw,bh,(Color){10,16,36,255});
            DrawRectangleLines(bx,by2,bw,bh,C_BD_MID);
            corner_marks(bx,by2,bw,bh,C_CYAN,14);

            const char *d1="DROP ASSEMBLY FILE HERE";
            const char *d2=".txt  or  .asm";
            T(d1, cx-TW(d1,16)/2, by2+22, 16, C_TXT_HI);
            T(d2, cx-TW(d2,11)/2, by2+46, 11, C_TXT_LO);

            DrawLine(bx+28, by2+70, bx+bw-28, by2+70, C_BD_LO);

            const char *k1="SPC  step forward     ←  step back     P  play / pause     R  reset";
            T(k1, cx-TW(k1,10)/2, by2+82, 10, C_TXT_MIN);

            const char *k2="Von Neumann  ·  5-stage pipeline  ·  12-instruction ISA";
            T(k2, cx-TW(k2,9)/2, by2+112, 9, (Color){28,44,80,255});
        }

        draw_botbar();
        EndDrawing();
    }

    if (G_ok) UnloadFont(G);
    CloseWindow();
    return 0;
}
