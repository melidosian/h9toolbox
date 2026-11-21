#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "radio_system.h"
#include "../modules/aprs/aprs_resident_config.h"

static const unsigned *keys;
static unsigned resets, reset_kind, reloads, queries, writes, busy, fail_save, tick, saw_module;
static uint8_t ring[2048];
static unsigned produced;
static void identity(struct radio_system_identity *id)
{ *id = (struct radio_system_identity){ .region=1, .base="V1.0.38", .hardware="V1.0.0", .ani="123" }; }
static int reload(void) { ++reloads; return 0; }
static int reset(unsigned kind) { ++resets; reset_kind = kind; return 0; }
static void query(void)
{
    static const uint8_t response[] = {0xc0,0x21,0,'V','1','.','0','.','1','5','X',0,'V','1','.','0','.','0',0,0xc0};
    ++queries;
    for (unsigned i=0;i<sizeof response;++i) ring[produced++ & 2047] = response[i];
}
static unsigned ring_end(void) { return produced & 2047; }
static unsigned ring_byte(unsigned at) { return ring[at & 2047]; }
static unsigned ticks(void) { return tick; }
const struct radio_system_platform radio_system_platform = {identity,reload,reset,query,ring_end,ring_byte,ticks};
static int read_station(unsigned offset,void *dst,unsigned length)
{ assert(offset==0x307d && length==7); memcpy(dst,"K9ABC\0\3",7); return 0; }
const struct radio_storage radio_stock_storage = {.read=read_station};
static unsigned status(struct h9_runtime_status *s)
{ *s = (struct h9_runtime_status){ .settings={.armed=1,.mute=1,.minutes=30} }; return 1; }
static void stock_tx(const void *arg,unsigned n,void (*run)(const void *,unsigned)) { if (!busy) run(arg,n); }
static const struct h9_runtime_api runtime = {.abi=H9_RUNTIME_ABI,.read_status=status,.stock_tx=stock_tx};
static const struct h9_runtime_prefix prefix = {.api=&runtime};
static const struct h9_api loader = {.version=5,.firmware=0x000105,.resident=&prefix};
int aprs_settings_save(const struct h9_api *api,const struct h9_runtime_api *service,
                       const struct ts_settings *next,struct ts_settings *saved)
{
    assert(api==&loader && service==&runtime);
    assert(!next->armed && !next->mute && next->minutes==TS_DEFAULT_MINUTES);
    if (fail_save) return 0;
    ++writes; *saved=*next; return 1;
}
unsigned gfx_wait_key(void) { assert(keys && *keys); return *keys++; }
void gfx_wait_release(void) { }
unsigned h9_key(void) { return tick>4 ? H9_KEY_BACK : 0; }
void h9_delay(unsigned n) { tick += n/10; assert(tick<400); }
void gfx_fill(unsigned x,unsigned y,unsigned w,unsigned h,uint16_t color)
{ (void)color; assert(x+w<=160 && y+h<=128); }
unsigned gfx_text(unsigned x,unsigned y,const char *s,unsigned max,unsigned scale,uint16_t fg,uint16_t bg)
{
    (void)fg;(void)bg;
    unsigned n=0; while(n<max&&s[n])++n;
    assert(x+n*6*scale<=160 && y+8*scale<=128);
    saw_module |= !strcmp(s,"V1.0.15X");
    return x+n*6*scale;
}
void h9_lcd_window(unsigned x0,unsigned y0,unsigned x1,unsigned y1)
{ assert(x0==45 && y0==23 && x1==114 && y1==92); }
void h9_lcd_write(const void *p,unsigned n) { assert(p && n && n<=128 && !(n&1)); }
int main(void)
{
    static const unsigned cancel[]={H9_KEY_BACK}, confirm[]={H9_KEY_MENU};
    for(unsigned action=RADIO_SYS_RESET_VFO;action<=RADIO_SYS_RESET_APRS;++action) {
        keys=cancel; assert(radio_system(&loader,action)==0 && !resets && !writes);
    }
    busy=1;keys=confirm;
    assert(radio_system(&loader,RADIO_SYS_RESET_ALL)==RADIO_BUSY && !resets && !writes);
    assert(radio_apply_pending(&loader)==RADIO_BUSY && !reloads);
    busy=0;
    for(unsigned action=RADIO_SYS_RESET_VFO;action<=RADIO_SYS_RESET_APRS;++action) {
        keys=confirm; assert(radio_system(&loader,action)==-1);
        assert(reset_kind==action-RADIO_SYS_RESET_VFO && resets==action-RADIO_SYS_RESET_VFO+1);
    }
    assert(writes==2 && radio_apply_pending(&loader)==0 && reloads==1);
    fail_save=1; keys=confirm;
    assert(radio_system(&loader,RADIO_SYS_RESET_APRS)==RADIO_IO_ERROR && resets==5 && writes==2);
    keys=cancel; assert(radio_system(&loader,RADIO_SYS_INFO)==0);
    const unsigned pages[]={H9_KEY_DOWN,H9_KEY_MENU,H9_KEY_UP,H9_KEY_BACK};
    keys=pages; assert(radio_system(&loader,RADIO_SYS_MANUAL)==0);
    assert(radio_system(&loader,RADIO_SYS_APRS_INFO)==0 && queries==1 && saw_module);
    struct radio_version_parser parser={0};
    const uint8_t bad[]={0xc0,0x21,0,'V',0xdb,0,0xc0};
    for(unsigned i=0;i<sizeof bad;++i) assert(!radio_version_feed(&parser,bad[i]));
    assert(!parser.module[0]);
    produced=2040; query();
    unsigned complete=0;
    for(unsigned at=2040;at<produced;++at) complete+=radio_version_feed(&parser,ring[at&2047]);
    assert(complete==1 && !strcmp(parser.module,"V1.0.15X") && !strcmp(parser.protocol,"V1.0.0"));
    for(unsigned i=0;i<60;++i) radio_version_feed(&parser,'X');
    assert(!radio_version_feed(&parser,0xc0));
    puts("System reset confirmations, TX reservation, live reload, information, QR pages and modem version parsing passed.");
}
