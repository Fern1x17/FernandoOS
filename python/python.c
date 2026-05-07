/*
 * FernandoOS — intérprete Python mínimo
 *
 * Soporta: variables int/str, print(), input(), str(), int(), len(), abs(),
 *          if/elif/else, while, for/in/range(), break, continue, pass,
 *          operadores aritméticos (+ - * // %), comparaciones, and/or/not,
 *          asignación = y aumentada += -= *= //= %=, concatenación de cadenas.
 */

#include "python.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../libc/string.h"
#include <stdint.h>

/* ── Configuración ────────────────────────────────────────── */
#define PY_MAX_LINES  256
#define PY_MAX_VARS    32
#define PY_STR_MAX    256
#define PY_VAR_NM      32
#define PY_LINE_MAX   256

/* ── Tipos de valor ───────────────────────────────────────── */
#define TY_INT  0
#define TY_STR  1
#define TY_NONE 2

typedef struct { int type; int32_t iv; char sv[PY_STR_MAX]; } Val;

static Val mk_int(int32_t v)     { Val r; r.type=TY_INT; r.iv=v; r.sv[0]='\0'; return r; }
static Val mk_none(void)         { Val r; r.type=TY_NONE; r.iv=0; r.sv[0]='\0'; return r; }
static Val mk_str(const char* s) {
    Val r; r.type=TY_STR; r.iv=0;
    strncpy(r.sv,s,PY_STR_MAX-1); r.sv[PY_STR_MAX-1]='\0';
    return r;
}
static int truthy(Val v) {
    if (v.type==TY_INT) return v.iv!=0;
    if (v.type==TY_STR) return v.sv[0]!='\0';
    return 0;
}

/* ── Variables globales ───────────────────────────────────── */
typedef struct { char nm[PY_VAR_NM]; Val v; int used; } Var;
static Var gvars[PY_MAX_VARS];

static void vset(const char* n, Val v) {
    for (int i=0;i<PY_MAX_VARS;i++)
        if (gvars[i].used && strcmp(gvars[i].nm,n)==0) { gvars[i].v=v; return; }
    for (int i=0;i<PY_MAX_VARS;i++)
        if (!gvars[i].used) {
            gvars[i].used=1;
            strncpy(gvars[i].nm,n,PY_VAR_NM-1);
            gvars[i].nm[PY_VAR_NM-1]='\0';
            gvars[i].v=v; return;
        }
}
static Val vget(const char* n) {
    for (int i=0;i<PY_MAX_VARS;i++)
        if (gvars[i].used && strcmp(gvars[i].nm,n)==0) return gvars[i].v;
    return mk_none();
}

/* ── Líneas de código ─────────────────────────────────────── */
typedef struct { int indent; char text[PY_LINE_MAX]; } Line;
static Line plines[PY_MAX_LINES];
static int  pnlines;

/* ── Flags de control de flujo ────────────────────────────── */
static int py_err  = 0;
static int py_brk  = 0;
static int py_cont = 0;

static void py_error(const char* msg) {
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    vga_puts("  Error Python: "); vga_puts(msg); vga_putchar('\n');
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    py_err=1;
}

/* ── Utilidades ───────────────────────────────────────────── */
static int is_alpha(char c){ return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'; }
static int is_digit(char c){ return c>='0'&&c<='9'; }
static int is_alnum(char c){ return is_alpha(c)||is_digit(c); }
static void skipws(const char* s, int* p){ while(s[*p]==' '||s[*p]=='\t') (*p)++; }

static void print_val(Val v) {
    if (v.type==TY_INT) {
        char buf[32]; itoa((int)v.iv, buf, 10); vga_puts(buf);
    } else if (v.type==TY_STR) {
        vga_puts(v.sv);
    } else {
        vga_puts("None");
    }
}

/* Concatenación segura en dst (máx dstsz bytes totales) */
static void scat(char* dst, const char* src, int dstsz) {
    int i=(int)strlen(dst), j=0;
    while (src[j] && i<dstsz-1) dst[i++]=src[j++];
    dst[i]='\0';
}

/* ── Evaluador de expresiones ─────────────────────────────── */
static Val eval_expr(const char* s, int* p);   /* forward */

static Val eval_atom(const char* s, int* p) {
    skipws(s,p);
    char c=s[*p];

    /* Literal de cadena */
    if (c=='"'||c=='\'') {
        char q=c; (*p)++;
        char buf[PY_STR_MAX]; int i=0;
        while (s[*p]&&s[*p]!=q&&i<PY_STR_MAX-1) {
            if (s[*p]=='\\'&&s[*p+1]) {
                (*p)++;
                switch(s[(*p)++]) {
                    case 'n':  buf[i++]='\n'; break;
                    case 't':  buf[i++]='\t'; break;
                    case '\\': buf[i++]='\\'; break;
                    case '"':  buf[i++]='"';  break;
                    case '\'': buf[i++]='\''; break;
                    default:   buf[i++]='?';  break;
                }
            } else buf[i++]=s[(*p)++];
        }
        buf[i]='\0';
        if (s[*p]==q) (*p)++;
        return mk_str(buf);
    }

    /* Expresión entre paréntesis */
    if (c=='(') {
        (*p)++;
        Val v=eval_expr(s,p);
        skipws(s,p);
        if (s[*p]==')') (*p)++;
        return v;
    }

    /* Literal entero */
    if (is_digit(c)) {
        int32_t v=0;
        while (is_digit(s[*p])) v=v*10+(int32_t)(s[(*p)++]-'0');
        return mk_int(v);
    }

    /* Identificador, palabra clave o llamada a función */
    if (is_alpha(c)) {
        char id[PY_VAR_NM]; int i=0;
        while (is_alnum(s[*p])&&i<PY_VAR_NM-1) id[i++]=s[(*p)++];
        id[i]='\0';

        if (strcmp(id,"True")==0)  return mk_int(1);
        if (strcmp(id,"False")==0) return mk_int(0);
        if (strcmp(id,"None")==0)  return mk_none();

        skipws(s,p);
        if (s[*p]=='(') {
            (*p)++;
            Val args[8]; int na=0;
            skipws(s,p);
            while (s[*p]!=')'&&s[*p]!='\0'&&!py_err&&na<8) {
                args[na++]=eval_expr(s,p);
                skipws(s,p);
                if (s[*p]==',') { (*p)++; skipws(s,p); }
            }
            if (s[*p]==')') (*p)++;

            if (strcmp(id,"str")==0) {
                if (na>0) {
                    if (args[0].type==TY_STR) return args[0];
                    char buf[32]; itoa((int)args[0].iv,buf,10); return mk_str(buf);
                }
                return mk_str("");
            }
            if (strcmp(id,"int")==0) {
                if (na>0) {
                    if (args[0].type==TY_INT) return args[0];
                    int sign=1,j=0;
                    if (args[0].sv[j]=='-'){sign=-1;j++;}
                    int32_t v=0;
                    while (is_digit(args[0].sv[j])) v=v*10+(int32_t)(args[0].sv[j++]-'0');
                    return mk_int((int32_t)sign*v);
                }
                return mk_int(0);
            }
            if (strcmp(id,"len")==0) {
                if (na>0&&args[0].type==TY_STR)
                    return mk_int((int32_t)strlen(args[0].sv));
                return mk_int(0);
            }
            if (strcmp(id,"abs")==0) {
                if (na>0&&args[0].type==TY_INT)
                    return mk_int(args[0].iv<0?-args[0].iv:args[0].iv);
                return mk_int(0);
            }
            if (strcmp(id,"input")==0) {
                if (na>0) print_val(args[0]);
                char buf[PY_STR_MAX]; int bi=0;
                vga_set_color(VGA_COLOR_YELLOW,VGA_COLOR_BLACK);
                while (bi<PY_STR_MAX-1) {
                    char ch=keyboard_getchar();
                    if (ch=='\n'||ch=='\r') { vga_putchar('\n'); break; }
                    if (ch=='\b') {
                        if (bi>0){ bi--; vga_putchar('\b'); vga_putchar(' '); vga_putchar('\b'); }
                    } else { buf[bi++]=ch; vga_putchar(ch); }
                }
                buf[bi]='\0';
                vga_set_color(VGA_COLOR_WHITE,VGA_COLOR_BLACK);
                return mk_str(buf);
            }
            if (strcmp(id,"print")==0) {
                for (int k=0;k<na;k++) { if (k>0) vga_putchar(' '); print_val(args[k]); }
                vga_putchar('\n');
                return mk_none();
            }
            if (strcmp(id,"range")==0) return mk_none();  /* solo para for-loops */

            /* Función desconocida */
            vga_set_color(VGA_COLOR_LIGHT_RED,VGA_COLOR_BLACK);
            vga_puts("  Error Python: funcion desconocida: "); vga_puts(id); vga_putchar('\n');
            vga_set_color(VGA_COLOR_WHITE,VGA_COLOR_BLACK);
            py_err=1;
            return mk_none();
        }

        return vget(id);
    }

    return mk_none();
}

/* Negación unaria */
static Val eval_unary(const char* s, int* p) {
    skipws(s,p);
    if (s[*p]=='-') { (*p)++; Val v=eval_atom(s,p); return v.type==TY_INT?mk_int(-v.iv):v; }
    if (s[*p]=='+') { (*p)++; }
    return eval_atom(s,p);
}

/* Multiplicación, división entera, módulo */
static Val eval_mul(const char* s, int* p) {
    Val left=eval_unary(s,p);
    skipws(s,p);
    while (!py_err) {
        char op=s[*p];
        int fl=(op=='/'&&s[*p+1]=='/');
        if (op=='*'||fl||op=='%') {
            if (fl) (*p)+=2; else (*p)++;
            Val right=eval_unary(s,p);
            if (left.type==TY_INT&&right.type==TY_INT) {
                if (op=='*') left=mk_int(left.iv*right.iv);
                else if (op=='%') left=mk_int(left.iv%right.iv);
                else {
                    if (right.iv==0){py_error("division por cero");return mk_none();}
                    left=mk_int(left.iv/right.iv);
                }
            }
            skipws(s,p);
        } else break;
    }
    return left;
}

/* Suma, resta, concatenación de cadenas */
static Val eval_add(const char* s, int* p) {
    Val left=eval_mul(s,p);
    skipws(s,p);
    while (!py_err&&(s[*p]=='+'||s[*p]=='-')) {
        char op=s[(*p)++];
        Val right=eval_mul(s,p);
        if (left.type==TY_INT&&right.type==TY_INT) {
            left=mk_int(op=='+'?left.iv+right.iv:left.iv-right.iv);
        } else if (op=='+') {
            char buf[PY_STR_MAX]; buf[0]='\0'; char tmp[32];
            if (left.type==TY_STR) strncpy(buf,left.sv,PY_STR_MAX-1);
            else { itoa((int)left.iv,tmp,10); scat(buf,tmp,PY_STR_MAX); }
            if (right.type==TY_STR) scat(buf,right.sv,PY_STR_MAX);
            else { itoa((int)right.iv,tmp,10); scat(buf,tmp,PY_STR_MAX); }
            left=mk_str(buf);
        }
        skipws(s,p);
    }
    return left;
}

/* Comparaciones */
static Val eval_cmp(const char* s, int* p) {
    Val left=eval_add(s,p);
    skipws(s,p);
    while (!py_err) {
        int op=-1;
        if      (s[*p]=='='&&s[*p+1]=='=') { (*p)+=2; op=0; }
        else if (s[*p]=='!'&&s[*p+1]=='=') { (*p)+=2; op=1; }
        else if (s[*p]=='<'&&s[*p+1]=='=') { (*p)+=2; op=2; }
        else if (s[*p]=='>'&&s[*p+1]=='=') { (*p)+=2; op=3; }
        else if (s[*p]=='<'&&s[*p+1]!='<') { (*p)++;  op=4; }
        else if (s[*p]=='>'&&s[*p+1]!='>') { (*p)++;  op=5; }
        else break;
        Val right=eval_add(s,p);
        int r=0;
        if (left.type==TY_INT&&right.type==TY_INT) {
            switch(op){case 0:r=left.iv==right.iv;break;case 1:r=left.iv!=right.iv;break;
                        case 2:r=left.iv<=right.iv;break;case 3:r=left.iv>=right.iv;break;
                        case 4:r=left.iv<right.iv; break;case 5:r=left.iv>right.iv; break;}
        } else if (left.type==TY_STR&&right.type==TY_STR) {
            int c=strcmp(left.sv,right.sv);
            switch(op){case 0:r=c==0;break;case 1:r=c!=0;break;case 2:r=c<=0;break;
                        case 3:r=c>=0;break;case 4:r=c<0; break;case 5:r=c>0; break;}
        }
        left=mk_int(r);
        skipws(s,p);
    }
    return left;
}

/* not */
static Val eval_not(const char* s, int* p) {
    skipws(s,p);
    if (strncmp(s+*p,"not",3)==0&&!is_alnum(s[*p+3])) {
        (*p)+=3; return mk_int(!truthy(eval_not(s,p)));
    }
    return eval_cmp(s,p);
}

/* and */
static Val eval_and(const char* s, int* p) {
    Val left=eval_not(s,p); skipws(s,p);
    while (!py_err&&strncmp(s+*p,"and",3)==0&&!is_alnum(s[*p+3])) {
        (*p)+=3;
        Val right=eval_not(s,p);
        left=mk_int(truthy(left)&&truthy(right));
        skipws(s,p);
    }
    return left;
}

/* or — nivel más alto */
static Val eval_or(const char* s, int* p) {
    Val left=eval_and(s,p); skipws(s,p);
    while (!py_err&&strncmp(s+*p,"or",2)==0&&!is_alnum(s[*p+2])) {
        (*p)+=2;
        Val right=eval_and(s,p);
        left=mk_int(truthy(left)||truthy(right));
        skipws(s,p);
    }
    return left;
}

static Val eval_expr(const char* s, int* p) { return eval_or(s,p); }

/* Evalúa una cadena completa como expresión */
static Val peval(const char* s) { int p=0; return eval_expr(s,&p); }

/* ── Auxiliares de bloques ────────────────────────────────── */

/* Primer índice >= start con indent <= base */
static int blk_end(int start, int base) {
    while (start<pnlines&&plines[start].indent>base) start++;
    return start;
}

/* Extrae text[p..último ':'] en buf (sin el colon). Retorna 1 si OK. */
static int extract_to_colon(const char* text, int p, char* buf) {
    int len=(int)strlen(text), cp=len-1;
    while (cp>=p&&(text[cp]==' '||text[cp]=='\t')) cp--;
    if (cp<p||text[cp]!=':') return 0;
    int clen=cp-p;
    while (clen>0&&(text[p+clen-1]==' '||text[p+clen-1]=='\t')) clen--;
    strncpy(buf,text+p,(uint32_t)clen);
    buf[clen]='\0';
    return 1;
}

/* ── Ejecución de sentencias ──────────────────────────────── */
static int exec_block(int start, int end, int indent);   /* forward */

static void exec_stmt(const char* text) {
    if (py_err) return;
    int p=0; skipws(text,&p);

    if (strncmp(text+p,"pass",4)==0&&!is_alnum(text[p+4])) return;
    if (strncmp(text+p,"break",5)==0&&!is_alnum(text[p+5]))    { py_brk=1;  return; }
    if (strncmp(text+p,"continue",8)==0&&!is_alnum(text[p+8])) { py_cont=1; return; }

    /* print(...) */
    if (strncmp(text+p,"print",5)==0&&text[p+5]=='(') {
        p+=6;
        int first=1; skipws(text,&p);
        while (text[p]!=')'&&text[p]!='\0'&&!py_err) {
            if (!first) vga_putchar(' ');
            first=0;
            print_val(eval_expr(text,&p));
            skipws(text,&p);
            if (text[p]==',') { p++; skipws(text,&p); }
        }
        if (text[p]==')') p++;
        vga_putchar('\n');
        return;
    }

    /* Asignación / asignación aumentada: ident [op]= expr */
    int pp=p;
    if (is_alpha(text[pp])) {
        char id[PY_VAR_NM]; int ip=0;
        while (is_alnum(text[pp])&&ip<PY_VAR_NM-1) id[ip++]=text[pp++];
        id[ip]='\0';
        skipws(text,&pp);

        /* Asignación simple */
        if (text[pp]=='='&&text[pp+1]!='=') {
            pp++;
            Val v=eval_expr(text,&pp);
            if (!py_err) vset(id,v);
            return;
        }
        /* Asignación aumentada: += -= *= //= %= */
        char aop=0; int aoplen=1;
        if      (text[pp]=='/'&&text[pp+1]=='/'&&text[pp+2]=='=') { aop='/'; aoplen=2; }
        else if (text[pp]=='+'&&text[pp+1]=='=') aop='+';
        else if (text[pp]=='-'&&text[pp+1]=='=') aop='-';
        else if (text[pp]=='*'&&text[pp+1]=='=') aop='*';
        else if (text[pp]=='%'&&text[pp+1]=='=') aop='%';
        if (aop) {
            pp+=aoplen+1;
            Val old=vget(id), rhs=eval_expr(text,&pp);
            if (!py_err) {
                Val nv=old;
                if (old.type==TY_INT&&rhs.type==TY_INT) {
                    switch(aop) {
                        case '+': nv=mk_int(old.iv+rhs.iv); break;
                        case '-': nv=mk_int(old.iv-rhs.iv); break;
                        case '*': nv=mk_int(old.iv*rhs.iv); break;
                        case '%': nv=mk_int(old.iv%rhs.iv); break;
                        default:
                            if(rhs.iv==0){py_error("division por cero");return;}
                            nv=mk_int(old.iv/rhs.iv); break;
                    }
                } else if (aop=='+'&&old.type==TY_STR&&rhs.type==TY_STR) {
                    char buf[PY_STR_MAX];
                    strncpy(buf,old.sv,PY_STR_MAX-1);
                    scat(buf,rhs.sv,PY_STR_MAX);
                    nv=mk_str(buf);
                }
                vset(id,nv);
            }
            return;
        }
    }

    /* Sentencia de expresión (p.ej. llamada a función) */
    eval_expr(text,&p);
}

/* Ejecuta líneas [start, end) con ese nivel de indentación */
static int exec_block(int start, int end, int indent) {
    int i=start;
    while (i<end&&!py_err&&!py_brk&&!py_cont) {
        if (plines[i].indent!=indent) { i++; continue; }
        const char* text=plines[i].text;
        int p=0; skipws(text,&p);

        /* ── if ───────────────────────────────────────── */
        if (strncmp(text+p,"if",2)==0&&!is_alnum(text[p+2])) {
            p+=2; skipws(text,&p);
            char cond[PY_LINE_MAX];
            if (!extract_to_colon(text,p,cond)) { py_error("falta ':' en if"); i++; continue; }
            int met=truthy(peval(cond));
            int bs=i+1, be=blk_end(bs,indent);
            if (met&&!py_err&&bs<be) exec_block(bs,be,plines[bs].indent);
            int j=be;
            while (j<end&&!py_err) {
                if (plines[j].indent!=indent) break;
                int jp=0; skipws(plines[j].text,&jp);
                if (strncmp(plines[j].text+jp,"elif",4)==0&&!is_alnum(plines[j].text[jp+4])) {
                    int ebs=j+1, ebe=blk_end(ebs,indent);
                    if (!met&&!py_err) {
                        jp+=4; skipws(plines[j].text,&jp);
                        char ec[PY_LINE_MAX];
                        if (extract_to_colon(plines[j].text,jp,ec)&&truthy(peval(ec))&&!py_err) {
                            met=1;
                            if (ebs<ebe) exec_block(ebs,ebe,plines[ebs].indent);
                        }
                    }
                    j=ebe;
                } else if (strncmp(plines[j].text+jp,"else:",5)==0) {
                    int ebs=j+1, ebe=blk_end(ebs,indent);
                    if (!met&&!py_err&&ebs<ebe) exec_block(ebs,ebe,plines[ebs].indent);
                    j=ebe; break;
                } else break;
            }
            i=j; continue;
        }

        /* ── while ────────────────────────────────────── */
        if (strncmp(text+p,"while",5)==0&&!is_alnum(text[p+5])) {
            p+=5; skipws(text,&p);
            char cond[PY_LINE_MAX];
            if (!extract_to_colon(text,p,cond)) { py_error("falta ':' en while"); i++; continue; }
            int bs=i+1, be=blk_end(bs,indent);
            int bi=(bs<be)?plines[bs].indent:indent+4;
            while (!py_err&&truthy(peval(cond))) {
                py_cont=0;
                exec_block(bs,be,bi);
                if (py_brk) { py_brk=0; break; }
                if (py_cont) { py_cont=0; continue; }
            }
            i=be; continue;
        }

        /* ── for var in range(a[,b[,c]]) ─────────────── */
        if (strncmp(text+p,"for",3)==0&&!is_alnum(text[p+3])) {
            p+=3; skipws(text,&p);
            char var[PY_VAR_NM]; int vi=0;
            while (is_alnum(text[p])&&vi<PY_VAR_NM-1) var[vi++]=text[p++];
            var[vi]='\0';
            skipws(text,&p);
            if (strncmp(text+p,"in",2)!=0||is_alnum(text[p+2])) {
                py_error("esperaba 'in' en for"); i++; continue;
            }
            p+=2; skipws(text,&p);
            if (strncmp(text+p,"range(",6)==0) {
                p+=6;
                Val a=eval_expr(text,&p); skipws(text,&p);
                Val b=a; int has_b=0;
                Val step=mk_int(1);
                if (text[p]==',') { p++; b=eval_expr(text,&p); has_b=1; skipws(text,&p); }
                if (text[p]==',') { p++; step=eval_expr(text,&p); skipws(text,&p); }
                if (text[p]==')') p++;
                int32_t fr=has_b?a.iv:0, to=has_b?b.iv:a.iv, st=step.iv;
                if (st==0) { py_error("range: step=0"); i++; continue; }
                int bs=i+1, be=blk_end(bs,indent);
                int bi=(bs<be)?plines[bs].indent:indent+4;
                for (int32_t k=fr; st>0?(k<to):(k>to); k+=st) {
                    if (py_err) break;
                    vset(var,mk_int(k));
                    py_cont=0;
                    exec_block(bs,be,bi);
                    if (py_brk) { py_brk=0; break; }
                    if (py_cont) { py_cont=0; continue; }
                }
                i=be; continue;
            } else {
                py_error("solo range() soportado en for");
                i++; continue;
            }
        }

        exec_stmt(text);
        i++;
    }
    return i;
}

/* ── Preprocesado: divide el fuente en líneas ─────────────── */
static void parse_src(const char* src) {
    pnlines=0;
    const char* p=src;
    while (*p&&pnlines<PY_MAX_LINES) {
        int indent=0;
        const char* lp=p;
        while (*lp==' '||*lp=='\t') { indent+=(*lp=='\t')?4:1; lp++; }
        if (*lp=='\0') break;
        if (*lp=='\n') { while(*p&&*p!='\n') p++; if(*p=='\n') p++; continue; }
        if (*lp=='#')  { while(*p&&*p!='\n') p++; if(*p=='\n') p++; continue; }
        plines[pnlines].indent=indent;
        p=lp;
        int i=0;
        int in_str=0; char str_ch=0;
        while (*p&&*p!='\n'&&*p!='\r'&&i<PY_LINE_MAX-1) {
            if (!in_str&&(*p=='"'||*p=='\'')) { in_str=1; str_ch=*p; }
            else if (in_str&&*p==str_ch)      { in_str=0; }
            else if (!in_str&&*p=='#')        break;
            plines[pnlines].text[i++]=*p++;
        }
        while (i>0&&(plines[pnlines].text[i-1]==' '||plines[pnlines].text[i-1]=='\t')) i--;
        plines[pnlines].text[i]='\0';
        if (i>0) pnlines++;
        while (*p&&*p!='\n') p++;
        if (*p=='\n') p++;
    }
}

/* ── Punto de entrada público ─────────────────────────────── */
void python_run(const char* source) {
    memset(gvars,0,sizeof(gvars));
    py_err=0; py_brk=0; py_cont=0; pnlines=0;
    parse_src(source);
    if (pnlines>0) exec_block(0,pnlines,plines[0].indent);
}
