#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "puzzles.h"

#include <amqp.h>
#include <amqp_tcp_socket.h>
#include "utils.h"

#include <glib.h>

#define CLASSNAME thegame.name

struct color {
    int r;
    int g;
    int b;
};

struct frontend {
    const game *game;
    midend *me;
    struct color *colours;
    int timer;
    struct preset_menu *preset_menu;
    struct preset_menuitemref *preset_menuitems;
    int n_preset_menuitems;
    struct font *fonts;
    int nfonts, fontsize;
    config_item *cfg;
    struct cfg_aux *cfgaux;
    int cfg_which, dlg_done;
    bool help_running;
    enum { DRAWING, PRINTING, NOTHING } drawstatus;
    int printcount, printw, printh;
    bool printsolns, printcurr, printcolour;
    float printscale;
    int printoffsetx, printoffsety;
    float printpixelscale;
    int fontstart;
    int linewidth;
    bool linedotted;
    drawing *dr;
    int xmin, ymin;
    float puzz_scale;
    GQueue * drawhandle;
};

void fatal(const char *fmt, ...){
    //printf("fatal, msg: %s\n", fmt);
    exit(1);
}
//TODO: Generate an actual random number
void get_random_seed(void **randseed, int *randseedsize)
{
    *randseed = malloc(sizeof(uint64_t));
    ((uint64_t * )(*randseed))[0] = now_microseconds();
    *randseedsize = sizeof(uint64_t);
}
void frontend_default_colour(frontend *fe, float *output)
{
    printf("frontend_default_colour\n");
    output[0] = 1.0;
    output[1] = 1.0;
    output[2] = 1.0;
}
void deactivate_timer(frontend *fe)
{
    if (!fe)
	return;			       /* for non-interactive midend */
    printf("deactivate_timer\n");
    fe->timer = 0;
}

void activate_timer(frontend *fe)
{
    printf("activate_timer\n");
    if (!fe)
	return;			       /* for non-interactive midend */
    if (!fe->timer) {
        return;
    }
}
static void msg_draw_text(void *handle, int x, int y, int fonttype,
			  int fontsize, int align, int colour,
                          const char *text)
{
    frontend *fe = (frontend *)handle;
    g_queue_push_tail(fe->drawhandle,g_strdup_printf("{\"draw\":\"text\",\n" 
            "\"x\": %d,\n"
            "\"y\": %d,\n" 
            "\"fonttype\": %d,\n"
            "\"fontsize\": %d,\n"
            "\"align\": %d,\n"
            "\"colour\": %d,\n"
            "\"text\": \"%s\"},\n",
            x,y,fonttype,fontsize,align,colour,text));
}

static void msg_draw_rect(void *handle, int x, int y, int w, int h, int colour)
{
    frontend *fe = (frontend *)handle;
    g_queue_push_tail(fe->drawhandle,g_strdup_printf("{\"draw\":\"rect\",\n"
            "\"x\": %d,\n"
            "\"y\": %d,\n"
            "\"w\": %d,\n"
            "\"h\": %d,\n"
            "\"colour\": %d},\n",
            x,y,w,h,colour));

}

static void msg_draw_line(void *handle, int x1, int y1, int x2, int y2, int colour)
{ 
    frontend *fe = (frontend *)handle;
    g_queue_push_tail(fe->drawhandle,g_strdup_printf("{\"draw\":\"line\",\n"
            "\"x1\": %d,\n"
            "\"y1\": %d,\n"
            "\"x2\": %d,\n"
            "\"y2\": %d,\n"
            "\"colour\": %d},\n",
            x1,y1,x2,y2,colour));
}

static void msg_draw_polygon(void *handle, int *coords, int npoints,
		     int fillcolour, int outlinecolour)
{ 
    frontend *fe = (frontend *)handle;
    //convert coords array to string
    char str[128];
    int i = 0;
    int index = 0;
    index += sprintf(&str[index],"[%d",coords[0]);
    for(i=1;i<2*npoints;i++){
        index += sprintf(&str[index], ",%d", coords[i]);
    }
    index += sprintf(&str[index], "]");
    g_queue_push_tail(fe->drawhandle,g_strdup_printf(
                "{\"draw\":\"polygon\",\n"
                "\"coords\": %s,\n"
                "\"npoints\": %d,\n"
                "\"fillcolour\": %d,\n"
                "\"outlinecolour\": %d},\n",
                str,npoints,fillcolour,outlinecolour));
}

static void msg_draw_circle(void *handle, int cx, int cy, int radius,
			 int fillcolour, int outlinecolour)
{
    frontend *fe = (frontend *)handle;
    g_queue_push_tail(fe->drawhandle,g_strdup_printf("{\"draw\":\"circle\",\n"
            "\"cx\": %d,\n"
            "\"cy\": %d,\n"
            "\"radius\": %d,\n"
            "\"fillcolour\": %d,\n"
            "\"outlinecolour\": %d},\n",
            cx,cy,radius,fillcolour,outlinecolour));
}

static void msg_draw_update(void *handle, int x, int y, int w, int h)
{
    frontend *fe = (frontend *)handle;
    g_queue_push_tail(fe->drawhandle,g_strdup_printf("{\"draw_update\": true,\n"
            "\"x\": %d,\n"
            "\"y\": %d,\n"
            "\"w\": %d,\n"
            "\"h\": %d,\n"
            "},\n",
            x,y,w,h));
}

static void msg_clip(void *handle, int x, int y, int w, int h)
{
    frontend *fe = (frontend *)handle;
    g_queue_push_tail(fe->drawhandle,g_strdup_printf("{\"clip\":true,\n"
            "\"x\": %d,\n"
            "\"y\": %d,\n"
            "\"w\": %d,\n"
            "\"h\": %d,\n"
            "\"cmds\":[\n",
            x,y,w,h));
}

static void msg_unclip(void *handle)
{
    frontend *fe = (frontend *)handle;
    g_queue_push_tail(fe->drawhandle,g_strdup_printf("]},\n"));
}

static void msg_start_draw(void *handle)
{
}

static void msg_end_draw(void *handle)
{
}

static void msg_status_bar(void *handle, const char *text)
{
    printf("status_bar, text: %s\t", text);
}

struct blitter {
   frontend *fe;
   int x, y, w, h;
};

static blitter *msg_blitter_new(void *handle, int w, int h){    
    fatal("blitter func called");
}

static void msg_blitter_free(void *handle, blitter *bl)
{
    fatal("blitter func called");
}
static void msg_blitter_save(void *handle, blitter *bl, int x, int y)
{
    fatal("blitter func called");
}

static void msg_blitter_load(void *handle, blitter *bl, int x, int y)
{
    fatal("blitter func called");
}

static void msg_begin_doc(void *handle, int pages)
{
    fatal("print func called");
}

static void msg_begin_page(void *handle, int number)
{
    fatal("print func called");
}

static void msg_begin_puzzle(void *handle, float xm, float xc,
			  float ym, float yc, int pw, int ph, float wmm)
{
    fatal("print func called");
}

static void msg_end_puzzle(void *handle)
{
    fatal("print func called");
}

static void msg_end_page(void *handle, int number)
{
    fatal("print func called");
}

static void msg_end_doc(void *handle)
{
    fatal("print func called");
}

static void msg_line_width(void *handle, float width)
{
    fatal("print func called");
}

static void msg_line_dotted(void *handle, bool dotted)
{
    fatal("print func called");
}

char *msg_text_fallback(void *handle, const char *const *strings, int nstrings)
{
    return dupstr(strings[0]);
}

const struct drawing_api msg_drawing = {
    msg_draw_text,
    msg_draw_rect,
    msg_draw_line,
    msg_draw_polygon,
    msg_draw_circle,
    msg_draw_update,
    msg_clip,
    msg_unclip,
    msg_start_draw,
    msg_end_draw,
    msg_status_bar,
    msg_blitter_new,
    msg_blitter_free,
    msg_blitter_save,
    msg_blitter_load,
    msg_begin_doc,
    msg_begin_page,
    msg_begin_puzzle,
    msg_end_puzzle,
    msg_end_page,
    msg_end_doc,
    msg_line_width,
    msg_line_dotted,
    msg_text_fallback,
};

static frontend *frontend_new()
{
    frontend *fe;
    const char *nogame = "Puzzles (no game selected)";

    fe = snew(frontend);

    fe->game = NULL;
    fe->me = NULL;

    fe->timer = 0;

    fe->help_running = false;

    fe->drawstatus = NOTHING;
    fe->dr = NULL;
    fe->fontstart = 0;

    fe->fonts = NULL;
    fe->nfonts = fe->fontsize = 0;

    fe->colours = NULL;

    fe->puzz_scale = 1.0;
    fe->drawhandle = NULL;

    return fe;
}

static bool savefile_read(void *wctx, void *buf, int len)
{
    FILE *fp = (FILE *)wctx;
    int ret;

    ret = fread(buf, 1, len, fp);
    return (ret == len);
}


static void get_max_puzzle_size(frontend *fe, int *x, int *y){
    *x = *y = INT_MAX;
}

static int fe_set_midend(frontend *fe, midend *me)
{
    int x, y;

    if (fe->me) {
        midend_free(fe->me);
        fe->preset_menu = NULL;
        sfree(fe->preset_menuitems);
    }
    fe->me = me;
    fe->game = midend_which_game(fe->me);

    {
	int i, ncolours;
        float *colours;

        colours = midend_colours(fe->me, &ncolours);

        if (fe->colours) sfree(fe->colours);

	fe->colours = snewn(ncolours,struct color);
	for (i = 0; i < ncolours; i++) {
        fe->colours->r = 255 * colours[i*3+0];
        fe->colours->g = 255 * colours[i*3+1];
        fe->colours->b = 255 * colours[i*3+2];
	}
        sfree(colours);
    }


    get_max_puzzle_size(fe, &x, &y);
    midend_size(fe->me, &x, &y, false);


    {

        fe->preset_menu = midend_get_presets(
            fe->me, &fe->n_preset_menuitems);
    }


    return 0;
}

void qconcat(void *str, void *vdest){
    char ** dest = (char **)vdest;
    char * olddest = *dest;
    *dest = g_strconcat(*dest,str,NULL);
    g_free(olddest);
}
char * queue_to_str(GQueue *queue){
    char *outstr;
    outstr = g_strdup("");
    g_queue_foreach(queue,qconcat,&outstr);
    return outstr;
}


bool str_is_prefixed(char * prefix, char * test, int maxlen){
    int i;
    for(i=0;i < maxlen;i++){
        if(!test[i]){
            return true;
        }
        if(prefix[i] != test[i]){
            return false;
        }
    }
    return true;
}

enum commands {
    SERVER_ERROR,
    SERVER_KILL,
    SERVER_NEW_GAME,
    SERVER_REDRAW_GAME,
    SERVER_UPDATE_GAME,
    SERVER_BATCH_UPDATE,
    SERVER_GET_CONFIG,
    SERVER_GET_COLORS,
    SERVER_GET_GAMEID,
    SERVER_ECHO,
    SERVER_PING
};

int getserverstate(amqp_message_t *message){
    // get message and decide on what state to be in
    if( str_is_prefixed("PINGXX",(char *)(message->body.bytes),6)){
        return SERVER_PING;
    }
    if( str_is_prefixed("NEWXXX",(char *)(message->body.bytes),6)){
        return SERVER_NEW_GAME;
    }
    if( str_is_prefixed("KILLXX",(char *)(message->body.bytes),6)){
        return SERVER_KILL;
    }
    if( str_is_prefixed("UPDATE",(char *)(message->body.bytes),6)){
        return SERVER_UPDATE_GAME;
    }
    if( str_is_prefixed("REDRAW",(char *)(message->body.bytes),6)){
        return SERVER_REDRAW_GAME;
    }
    if( str_is_prefixed("GETCFG",(char *)(message->body.bytes),6)){
        return SERVER_GET_CONFIG;
    }
    if( str_is_prefixed("BATCHX",(char *)(message->body.bytes),6)){
    	return SERVER_BATCH_UPDATE;
    }
    if( str_is_prefixed("COLORS",(char *)(message->body.bytes),6)){
    	return SERVER_GET_COLORS;
    }
    if( str_is_prefixed("GAMEID",(char *)(message->body.bytes),6)){
    	return SERVER_GET_GAMEID;
    }
    if( str_is_prefixed("ECHOXX",(char *)(message->body.bytes),6)){
    	return SERVER_ECHO;
    }
    printf("Malformed Message, Killing Server");
    return SERVER_KILL;
}

struct str_read_ctx {
    size_t pos;
    char **str;    
};

bool read_from_string(void * vctx, void * buf, int len){
    struct str_read_ctx * ctx = (struct str_read_ctx *)vctx;
    memcpy(buf,(*(ctx->str)+ctx->pos),len);
    ctx->pos+=len;
    return true;
}


void load_game(midend * me,char ** game_str){
    int x,y;
    struct str_read_ctx ctx;
    GRegex * unescape_newline = g_regex_new("\\\\n",0,0,NULL);
    char * unescaped = g_regex_replace_literal(unescape_newline,*game_str,-1,0,"\n",0,NULL);
    ctx.pos = 0;
    ctx.str = &unescaped;
    midend_deserialise(me,read_from_string,&ctx);
    x = INT_MAX;
    y = INT_MAX;
    midend_size(me, &x, &y, false);
}

struct str_write_ctx {
    size_t curr_size;
    char * buffer;
};

void write_to_str(void *vctx,const void *buf,int len){
    struct str_write_ctx *ctx = (struct str_write_ctx *)vctx;
    ctx->buffer = realloc(ctx->buffer,(ctx->curr_size)+len);
    memcpy((ctx->buffer)+(ctx->curr_size),buf,len);
    ctx->curr_size += len;
}

char * game_string(midend* me){
    struct str_write_ctx ctx;
    ctx.curr_size = 0;
    ctx.buffer = NULL;
    midend_serialise(me,write_to_str,&ctx);
    char * rawgamestring = g_strndup(ctx.buffer,ctx.curr_size);
    
    GRegex * fix_newlines = g_regex_new("\\n",0,0,NULL); 
    char * out = g_regex_replace_literal(fix_newlines,rawgamestring,-1,0,"\\\\n",0,NULL);

    g_regex_unref(fix_newlines);
    g_free(rawgamestring);
    free(ctx.buffer);
    return out;
}
char * draw_string(midend *me,frontend *fe, bool force){
    char * drawstring;
    int x,y;
    x = INT_MAX;
    y = INT_MAX;
    midend_size(me,&x,&y,false);
    fe->drawhandle = g_queue_new();
    g_queue_push_tail(fe->drawhandle,g_strdup_printf("{\"draw\":true,\"size\": {\"x\": %d, \"y\": %d,},\n \"cmds\":[\n",x,y));
    if(force){
        midend_force_redraw(me);
    } else {
        midend_redraw(me);
    }
    g_queue_push_tail(fe->drawhandle,g_strdup("]}"));
    drawstring = queue_to_str(fe->drawhandle);
    g_queue_free_full(fe->drawhandle,g_free);
    return drawstring;
}

const char * set_cfg(midend *me,char * csopts){
    char ** opts;
    char garbage[500];
    config_item * cfg;
    opts = g_strsplit(csopts,",",-1);
    cfg = midend_get_config(me,CFG_SETTINGS,(char **)&garbage);
    if(!cfg){
        return NULL;
    }
    int i;
    i = 0;
    while(cfg[i].type != C_END){
	if(!opts[i]){
            return "Not Enough Options";
        }
        switch(cfg[i].type){
            case C_STRING:
                cfg[i].u.string.sval = g_strdup(opts[i]);
                break;
            case C_BOOLEAN:
                cfg[i].u.boolean.bval = atoi(opts[i]);
                break;
            case C_CHOICES:
                cfg[i].u.choices.selected = atoi(opts[i]);
                break;
            default:
		return "Something went horribly wrong in config-setting";
        }
        i++;
    }
    const char * out = midend_set_config(me,CFG_SETTINGS,cfg);
    i = 0;
    free_cfg(cfg);
    g_strfreev(opts);
    return out;
}

void process_key_string(midend *me,frontend *fe,char * keystring){
    char ** keyparts;
    int x,y,b;
    sscanf(keystring, "%d,%d,%d",&x,&y,&b);
    int sx,sy;
    sx = INT_MAX;
    sy = INT_MAX;
    midend_size(me,&sx,&sy,false);
    fe->drawhandle = g_queue_new();
    midend_process_key(me,x,y,b);
    g_queue_free_full(fe->drawhandle,g_free);
}

void send_reply(amqp_connection_state_t conn, amqp_message_t *message,char * msg){
    GRegex * fix_trailing_commas = g_regex_new(",(\\s*[\\]}])",0,0,NULL);
    struct amqp_basic_properties_t_ reply_properties;
    reply_properties._flags = AMQP_BASIC_CORRELATION_ID_FLAG | AMQP_BASIC_EXPIRATION_FLAG;
    reply_properties.correlation_id = message->properties.correlation_id;
    reply_properties.expiration = amqp_cstring_bytes("5000");
    char * commas_fixed = g_regex_replace(fix_trailing_commas,msg,-1,0,"\\1",0,NULL);
    g_regex_unref(fix_trailing_commas); 
    printf("sending reply to %s\n",g_strndup(message->properties.reply_to.bytes,message->properties.reply_to.len));
    amqp_bytes_t msgbytes;
    msgbytes = amqp_cstring_bytes(commas_fixed);
    die_on_error(amqp_basic_publish(conn, 1, amqp_cstring_bytes(""),message->properties.reply_to,0,0,&reply_properties,msgbytes),"Failed to send message");
    g_free(commas_fixed);
}

int gameloop(amqp_connection_state_t pwq_conn,amqp_bytes_t *queue){
    printf("SERVER STARTING\n");
    //Setup For this function
    bool keepgoing = true;
    int i;

    //Setup For SimonTPPC
    char * starterror = NULL;
    const char * error = NULL;
    const game *gg;
    frontend *fe;
    midend *me;
    
    gg = &thegame;
    fe = frontend_new();

    me = midend_new(fe, gg, &msg_drawing, fe);
    midend_new_game(me);
    fe_set_midend(fe, me);
    
    if (!me) {
        char buf[128];
        sprintf(buf, "%.100s Error", gg->name);
        return 1;
    }

    int x = INT_MAX;
    int y = INT_MAX; 
    
    //Setup For Drawing
    GQueue *drawhandle;
    config_item *cfg;
    float * colors;
    int ncolors;
    char garbage[100];
    char * drawstring;
    char * gamestring;
    char * respstring;

    //Setup For Message Passing
    amqp_rpc_reply_t pwq_res;
    amqp_envelope_t env;
    amqp_envelope_t *pwq_envelope = &env;
    char * msg_text;
    char ** msg_parts;

    //Setup For Message construction
    const char * cfg_set_error;

    while(keepgoing){
        amqp_maybe_release_buffers(pwq_conn);
        pwq_res = 
        pwq_res = amqp_consume_message(pwq_conn, pwq_envelope, NULL, 0);
        amqp_basic_ack(pwq_conn,1,pwq_envelope->delivery_tag,false);
        if(pwq_res.reply_type != AMQP_RESPONSE_NORMAL){
            printf("errored %d\n",pwq_res.reply_type);
            amqp_destroy_envelope(pwq_envelope);
            continue;
        }
        msg_text = g_strndup((char *)(pwq_envelope->message.body.bytes),pwq_envelope->message.body.len);
        printf("MSG: %s\n",msg_text);
        msg_parts = g_strsplit(msg_text,"~",-1); 
        switch(getserverstate(&(pwq_envelope->message))){
            case SERVER_KILL: 
                keepgoing = false;
                send_reply(pwq_conn,&(pwq_envelope->message),"Adieu");
                break;
	    case SERVER_PING:
                send_reply(pwq_conn,&(pwq_envelope->message),"PONG");
            case SERVER_NEW_GAME:
                printf("NEW GAME\n");
                if(msg_parts[1] == NULL){
                    send_reply(pwq_conn,&(pwq_envelope->message),"MALFORMED MSG");
                    break;
                }
                cfg_set_error = set_cfg(me,msg_parts[1]);
		if(cfg_set_error != NULL){
			printf("cfg_set error: %s\n",cfg_set_error);
			break;
		}
		midend_new_game(me);
                drawstring = draw_string(me,fe,true);
                gamestring = game_string(me);
                respstring = g_strdup_printf("{\"draw\":%s,\n\"gamestate\":\"%s\"\n,}\n",drawstring,gamestring);
                send_reply(pwq_conn,&(pwq_envelope->message),respstring);
                g_free(drawstring);
                g_free(gamestring);
                g_free(respstring);
                break;
            case SERVER_UPDATE_GAME:
                printf("UPDATE GAME\n");
                if(msg_parts[1] == NULL || msg_parts[2] == NULL){
                    send_reply(pwq_conn,&(pwq_envelope->message),"MALFORMED MSG");
                    break;
                }
                load_game(me,&(msg_parts[1]));
                process_key_string(me,fe,msg_parts[2]);
                drawstring = draw_string(me,fe,false); 
                gamestring = game_string(me);
                respstring = g_strdup_printf("{\"draw\":%s,\n\"gamestate\":\"%s\"\n,\"status\":%d\n}\n",drawstring,gamestring,midend_status(me));
                send_reply(pwq_conn,&(pwq_envelope->message),respstring);
                g_free(drawstring);
                g_free(gamestring);
                g_free(respstring);
                break;
	    case SERVER_BATCH_UPDATE:
		if(msg_parts[1] == NULL || msg_parts[2] == NULL){
		    send_reply(pwq_conn,&(pwq_envelope->message),"MALFORMED MSG");
		}
                printf("bugid1: %s\n",midend_get_game_id(me));
                load_game(me,&(msg_parts[1]));
                printf("bugid2: %s\n",midend_get_game_id(me));
		for(i = 0; i < atoi(msg_parts[2]); i++){
		    if(!msg_parts[3+i]){
		    	send_reply(pwq_conn,&(pwq_envelope->message),"MALFORMED MSG");
		    }
		    printf("handling: %s\n",msg_parts[3+i]);
		    process_key_string(me,fe,msg_parts[3+i]);
		    printf("finished handling\n");
		}
                drawstring = draw_string(me,fe,false); 
                gamestring = game_string(me);
                respstring = g_strdup_printf("{\"draw\":%s,\n\"gamestate\":\"%s\"\n,\"status\":%d\n}\n",drawstring,gamestring,midend_status(me));
                send_reply(pwq_conn,&(pwq_envelope->message),respstring);
                g_free(drawstring);
                g_free(gamestring);
                g_free(respstring);
		break;
	    case SERVER_GET_GAMEID:
		printf("GET GAMEID\n");
                if(msg_parts[1] == NULL){
                    send_reply(pwq_conn,&(pwq_envelope->message),"MALFORMED MSG");
                    break;
                }
                load_game(me,&(msg_parts[1]));
                drawstring = midend_get_game_id(me);
                respstring = g_strdup_printf("{\"game_id\":\"%s\"}\n",drawstring);
		send_reply(pwq_conn,&(pwq_envelope->message),respstring);
		g_free(respstring);
		free(drawstring);
		break;
            case SERVER_REDRAW_GAME:
                printf("REDRAW GAME\n");
                if(msg_parts[1] == NULL){
                    send_reply(pwq_conn,&(pwq_envelope->message),"MALFORMED MSG");
                    break;
                }
		load_game(me,&(msg_parts[1]));
                drawstring = draw_string(me,fe,true); 
                gamestring = game_string(me);
                respstring = g_strdup_printf("{\"draw\":%s,\n\"gamestate\":\"%s\"\n,\"status\":%d\n}\n",drawstring,gamestring,midend_status(me));
		send_reply(pwq_conn,&(pwq_envelope->message),respstring);
                g_free(drawstring);
                g_free(gamestring);
                g_free(respstring);
                break;
            case SERVER_GET_CONFIG:
                printf("GET_CONFIG\n");
                cfg = midend_get_config(me,CFG_SETTINGS,(char **)&garbage);
                drawhandle = g_queue_new();
                g_queue_push_tail(drawhandle,g_strdup("{\"config\":true,\n\"opts\":[\n"));
                if(cfg != NULL){
                    i=0;
                    while(cfg[i].type != C_END){
                        drawstring = g_strescape(cfg[i].name,"");
			g_queue_push_tail(drawhandle,g_strdup_printf("{\"name\":\"%s\",\n",drawstring));
			g_free(drawstring);
                        if(cfg[i].type == C_STRING){
                            g_queue_push_tail(drawhandle,g_strdup("\"type\":\"string\",\n"));
                        } else if(cfg[i].type == C_BOOLEAN){
                            g_queue_push_tail(drawhandle,g_strdup("\"type\":\"boolean\",\n"));
                        } else if(cfg[i].type == C_CHOICES){
                            g_queue_push_tail(drawhandle,g_strdup("\"type\":\"choices\",\n"));
                            g_queue_push_tail(drawhandle,g_strdup_printf("\"choices\":\"%s\",\n",cfg[i].u.choices.choicenames));
                        }
                        g_queue_push_tail(drawhandle,g_strdup("},\n"));
                        i++;
                    }
                }
                g_queue_push_tail(drawhandle,g_strdup("]}"));
                drawstring = queue_to_str(drawhandle);
                send_reply(pwq_conn,&(pwq_envelope->message),drawstring);
                g_free(drawstring);
                g_queue_free_full(drawhandle,g_free);
                free_cfg(cfg);
                break;
            case SERVER_ECHO:
		printf("ECHOING: %s",msg_parts[1]);
                send_reply(pwq_conn,&(pwq_envelope->message),msg_parts[1]);
                break;
	    case SERVER_GET_COLORS:
		colors = midend_colours(me,&ncolors);
		drawhandle = g_queue_new();
		g_queue_push_tail(drawhandle,g_strdup("{\"colours\":[\n"));
		for(i = 0; i < ncolors; i++){
			if(i){
                		g_queue_push_tail(drawhandle,g_strdup(",\n"));
			}
			g_queue_push_tail(drawhandle,g_strdup_printf("{\"r\":%f,\"g\":%f,\"b\":%f}",colors[3*i],colors[3*i+1],colors[3*i+2]));
		}
		g_queue_push_tail(drawhandle,g_strdup("]}"));
                drawstring = queue_to_str(drawhandle);
                send_reply(pwq_conn,&(pwq_envelope->message),drawstring);
                g_free(drawstring);
                g_queue_free_full(drawhandle,g_free);
            case SERVER_ERROR:
            default :
                printf("SERVER ERROR\n");
        }
        g_free(msg_text);
        g_strfreev(msg_parts);
        amqp_destroy_envelope(pwq_envelope);
    }
     
    midend_free(me);
    sfree(fe);
    die_on_amqp_error(amqp_channel_close(pwq_conn, 1, AMQP_REPLY_SUCCESS),
                                 "Closing channel");
    die_on_amqp_error(amqp_connection_close(pwq_conn, AMQP_REPLY_SUCCESS),
                                   "Closing connection");
    die_on_error(amqp_destroy_connection(pwq_conn), "Ending connection");
    return 0;
}


int main(int argc, char*argv[]){    
    //For Puzzle Work Queue
	char const *pwq_hostname;
	int pwq_port, pwq_status;
	char const *pwq_queuename;
    amqp_bytes_t queue;
    char const *pwq_user, *pwq_pass;
    amqp_socket_t *pwq_socket = NULL;
	amqp_connection_state_t pwq_conn;
    //Handle Command Line Args
    if(argc % 2 != 1 || argc < 11){
        fprintf(stderr,"Usage requires the following tagged args: -host, -port, -queue, -user, -pass\n");
        return 1;
    }
   int i; 
    for(i=1;i < argc;i+=2){
	    if(g_str_equal(argv[i],"-host")){
	        pwq_hostname = argv[i+1];
        }
        if(g_str_equal(argv[i],"-port")){
            pwq_port = atoi(argv[i+1]);
        }
        if(g_str_equal(argv[i],"-queue")){
            pwq_queuename = argv[i+1];
        }
        if(g_str_equal(argv[i],"-user")){
            pwq_user = argv[i+1];
        }
        if(g_str_equal(argv[i],"-pass")){
            pwq_pass = argv[i+1];
        }
	    printf("arg %d = %s, arg %d + 1 = %s\n",i,argv[i],i,argv[i+1]);
    }
    
    pwq_conn = amqp_new_connection();
    pwq_socket = amqp_tcp_socket_new(pwq_conn);
    if (!pwq_socket){
        die("creating PWQ TCP socket");
    }
    
    pwq_status = amqp_socket_open(pwq_socket,pwq_hostname,pwq_port);
    if (pwq_status){
   	printf("pwq:status %d\n",pwq_status); 
	    die("opening PWQ TCP socket");
    }
    
    die_on_amqp_error(amqp_login(pwq_conn, "/", 0, 131072, 0,
                AMQP_SASL_METHOD_PLAIN, pwq_user, pwq_pass),
            "Logging In to PWQ");
    
    amqp_channel_open(pwq_conn, 1);
    die_on_amqp_error(amqp_get_rpc_reply(pwq_conn), "Opening PWQ channel");
    
    queue = amqp_cstring_bytes(pwq_queuename);
    amqp_queue_declare(pwq_conn,1,amqp_cstring_bytes(pwq_queuename),0,0,0,0,amqp_empty_table);
    die_on_amqp_error(amqp_get_rpc_reply(pwq_conn), "Declaring PWQ queue");

    amqp_basic_consume(pwq_conn, 1, amqp_cstring_bytes(pwq_queuename), amqp_empty_bytes,
                                 0, 0, 0, amqp_empty_table);
    die_on_amqp_error(amqp_get_rpc_reply(pwq_conn), "Consuming PWQ");

    return gameloop(pwq_conn,&queue);
}


