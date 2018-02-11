/* hdrs/defs.h */
/* External function declarations */

/* comm/admin.c */
extern void do_search(dbref player, char *arg1, char *arg3);
extern void do_find(dbref player, char *name);
extern void do_stats(dbref player, char *name);
extern void calc_stats(dbref owner, int *total, int *obj, int *pla);
extern void do_wipeout(dbref player, char *who, char *arg2);
extern void do_chownall(dbref player, char *arg1, char *arg2);
extern void do_cpage(dbref player, char *name, char *msg);
extern void do_cboot(dbref player, char *name);
extern void do_boot(dbref player, char *name, char *msg);
extern void do_join(dbref player, char *arg1);
extern void do_summon(dbref player, char *arg1);
extern void do_capture(dbref player, char *arg1, char *arg2);
extern void do_uncapture(dbref player, char *arg1);
extern int  inactive(dbref player);
extern void do_inactivity(dbref player, char *arg1);
extern int  inactive_nuke(void);
extern void do_laston(dbref player, char *name);
extern void do_pfile(dbref player, char *name, char *report);
extern void do_sitelock(dbref player, char *host, char *arg2);
extern void list_sitelocks(dbref player);
extern void save_sitelocks(FILE *f);
extern int  load_sitelocks(FILE *f);

/* comm/com.c */
extern int  is_on_channel(dbref player, char *channel);
extern void com_send(char *channel, char *message);
extern void do_com(dbref player, char *arg1, char *arg2);
extern void do_cemit(dbref player, char *channel, char *msg);
extern void remove_channel_type(dbref player, char type);
extern void do_channel(dbref player, char *arg1);
extern void do_comblock(dbref player, char *arg1, char *arg2);
extern void do_comvoice(dbref player, char *arg1, char *arg2);
extern void do_comlock(dbref player, char *arg1, char *arg2);
extern void list_comlocks(dbref player);
extern void save_comlocks(FILE *f);
extern int  load_comlocks(FILE *f);

/* comm/create.c */
extern void do_link(dbref player, char *arg1, char *arg2);
extern int  can_variable_link(dbref exit, dbref room);
extern dbref resolve_link(dbref player, dbref exit, int *plane);
extern void do_dig(dbref player, char *name, char *argc, char **argv);
extern void do_open(dbref player, char *arg1, char *arg2);
extern void do_create(dbref player, char *name);
extern void do_clone(dbref player, char *arg1, char *name);
extern void do_zone(dbref player, char *name, char *link);

/* comm/give.c */
extern void do_take(dbref player, char *obj);
extern void do_drop(dbref player, char *obj);
extern void do_give(dbref player, char *name, char *obj);
extern void do_put(dbref player, char *name, char *obj);
extern void do_grab(dbref player, char *arg1, char *obj);
extern void do_money(dbref player, char *arg1);
extern void do_charge(dbref player, char *arg1, char *arg2);

/* comm/look.c */
extern void do_look(dbref player, char *arg1);
extern void look_room(dbref player, dbref loc, int type);
extern int  can_see(dbref player, dbref thing);
extern void do_exits(dbref player, char *name);
extern ATTR *match_attr(dbref player, dbref thing, char *name, int type, char *value);
extern void do_examine(dbref player, char *name, char *arg2, char **argv, char *pure, dbref cause);
extern void do_inventory(dbref player);
extern void do_decompile(dbref player, char *name);
extern void db_print(FILE *f);
extern void do_whereis(dbref player, char *name);
extern void do_finger(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);

/* comm/move.c */
extern int  can_move(dbref player, dbref thing, dbref loc, int type);
extern void do_move(dbref player, char *direction);
extern void enter_room(dbref player, dbref loc);
extern void do_teleport(dbref player, char *arg1, char *arg2);
extern void do_enter(dbref player, char *what);
extern void do_leave(dbref player);
extern void do_run(dbref player);
extern void do_push(dbref player, char *arg1);
extern dbref remove_first(dbref first, dbref what);

/* comm/set.c */
extern int  match_controlled(dbref player, char *name, char power);
extern void do_name(dbref player, char *arg1, char *name);
extern void do_unlink(dbref player, char *arg1);
extern void do_chown(dbref player, char *name, char *newobj);
extern void do_unlock(dbref player, char *name);
extern void do_set(dbref player, char *name, char *flag);
extern int  test_set(dbref player, char *cmd, char *arg1, char *arg2);
extern void do_pennies(dbref player, char *arg1, char *arg2);
extern void do_edit(dbref player, char *name, char *argc, char **argv);
extern void do_cycle(dbref player, char *arg1, char *arg2, char **argv);
extern void do_wipeattr(dbref player, char *arg1, char *arg2);
extern void do_poll(dbref player, char *arg1, char *arg2, char **argv, char *pure);
extern void do_update(dbref player, char *arg1);
extern void do_plane(dbref player, char *arg1, char *arg2);

/* comm/speech.c */
extern int  pass_slock(dbref player, dbref room, int msg);
extern int  can_emit_msg(dbref player, dbref target, dbref loc, char *msg, int skip);
extern void notify(dbref player, char *fmt, ...) __attribute__((format(printf, 2, 3)));
extern void do_say(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_whisper(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_pose_cmd(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_pose(dbref player, dbref cause, char *msg, int type);
extern void do_echo(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_necho(dbref player, char *arg1, char *arg2, char **argv, char *pure);
extern void do_emit(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_nemit(dbref player, char *arg1, char *arg2, char **argv, char *pure);
extern void do_pemit(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_npemit(dbref player, char *arg1, char *arg2, char **argv, char *pure);
extern void do_remit(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_oemit(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_oremit(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_zemit(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_print(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void room_emote(dbref player, dbref cause, char *target, char *msg, int type);
extern void do_announce(dbref player, char *arg1, char *argc, char **argv, char *arg2);
extern void do_broadcast(dbref player, char *arg1, char *argc, char **argv, char *arg2);
extern void do_wemit(dbref player, char *arg1, char *argc, char **argv, char *arg2);
extern void do_output(dbref player, char *arg1, char *arg2);
extern void do_page(dbref player, char *arg1, char *arg2);
extern void notify_room(dbref player, dbref except, dbref except2, char *fmt, ...) __attribute__((format(printf, 4, 5)));
extern void notify_list(dbref *list, int inv, dbref except, char *fmt, ...) __attribute__((format(printf, 4, 5)));
extern void do_cnotify(dbref player, char *message, int loc);
extern void display_cnotify(dbref player);
extern void do_to(dbref player, char *arg1, char *arg2, char **argv, char *pure);
extern void do_talk(dbref player, char *arg1);

/* db/attrib.c */
extern void list_attributes(dbref player, char *name);
extern ATTR *builtin_atr_str(char *str);
extern ATTR *def_atr_str(dbref obj, char *str);
extern ATTR *atr_str(dbref obj, char *str);
extern ATTR *atr_num(dbref obj, unsigned char num);
extern void atr_add_obj(dbref thing, dbref obj, unsigned char num, char *str);
extern char *atr_get_obj(dbref thing, dbref obj, unsigned char num);
extern void atr_clean(dbref thing, dbref obj, int num);
extern void atr_cpy(dbref dest, dbref source);
extern struct all_atr_list *all_attributes(dbref thing);

/* db/compress.c */
extern void init_compress(void);
extern char *compress(unsigned char *s);
extern char *uncompress(unsigned char *s);

/* db/convert.c */
extern void mush_get_atrdefs(FILE *f);
extern void mush_free_atrdefs(void);
extern int  muse_db_read_object(dbref i, FILE *f);
extern int  mush_db_read_object(dbref i, FILE *f);
extern int  penn_db_read_object(dbref i, FILE *f);
extern void install_zones(void);
extern void install_parents(void);
extern void check_integrity(void);
extern void load_inzone_lists(void);

/* db/destroy.c */
extern void do_destroy(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause, int direct);
extern void do_undestroy(dbref player, char *arg1);
extern void recycle(dbref thing);
extern void validate_links(void);
extern int  db_check(void);
extern dbref get_upfront(void);
extern void do_upfront(dbref player, char *arg1);
extern dbref free_get(void);
extern void purge_going_objs(void);
extern void destroy_old_objs(void);
extern int  report_disconnected_rooms(dbref player);

/* db/inherit.c */
extern void do_defattr(dbref player, char *arg1, char *arg2);
extern void do_undefattr(dbref player, char *arg1);
extern void do_redefattr(dbref player, char *arg1, char *arg2);
extern char *unparse_attrflags(int flags);
extern int  is_a(dbref thing, dbref parent);
extern void do_addparent(dbref player, char *arg1, char *arg2);
extern void do_delparent(dbref player, char *arg1, char *arg2);
extern void do_addzone(dbref player, char *arg1, char *arg2);
extern void do_delzone(dbref player, char *arg1, char *arg2);
extern void push_first(dbref **list, dbref thing);
extern void push_last(dbref **list, dbref thing);
extern void pull_list(dbref **list, dbref thing);
extern dbref *copy_list(dbref *list);

/* db/load.c */
extern dbref new_object(void);
extern int  getref(FILE *f);
extern char *getstring_noalloc(FILE *f);
extern dbref *getlist(FILE *f);
extern char *get_db_version(void);
extern void db_error(char *message) __attribute__((__noreturn__));
extern void create_db(void);
extern int  load_database(void);

/* db/runtime.c */
extern void do_dump(dbref player);
extern void do_shutdown(dbref player);
extern void do_reboot(dbref player, char *options);
extern void dump_database(void);
extern void fork_and_dump(void);
extern void no_dbdump(int background);
extern void do_crash(dbref player);
extern void start_game(void);
extern void init_config(void);
extern void test_database(void);

/* db/save.c */
extern void putref(FILE *f, dbref num);
extern void putstring(FILE *f, char *s);
extern void putlist(FILE *f, dbref *list);
extern dbref db_write(FILE *f, char *filename);

/* game/command.c */
extern void list_commands(dbref player);
extern int  atr_match(dbref thing, dbref player, char type, char *str);
extern void process_command(dbref player, char *cmd, dbref cause, int direct);

/* game/help.c */
extern int  page_helpfile(DESC *d, char *msg);
extern void do_help(dbref player, char *req);

/* game/match.c */
extern dbref match_thing(dbref player, char *name, int type);
extern char *string_match(char *src, char *sub);
extern int  exit_alias(dbref thing, unsigned char attr, char *str);
extern int  match_word(char *list, char *word);
extern int  wild_match(char *d, char *s, int flags);

/* game/player.c */
extern void add_player(dbref player);
extern void delete_player(dbref player);
extern dbref lookup_player(char *name);
extern dbref create_player(char *name, char class);
extern void destroy_player(dbref victim);
extern int  boot_off(dbref player);
extern int  crypt_pass(dbref player, char *pass, int check);
extern void do_pcreate(dbref player, char *name);
extern void do_nuke(dbref player, char *name);
extern void list_players(dbref player, char *prefix);

/* game/powers.c */
extern void list_powers(dbref player, char *arg2);
extern int  controls(dbref player, dbref recipt, unsigned char pow);
extern void initialize_class(dbref player, char class);
extern void do_class(dbref player, char *arg1, char *arg2);
extern void do_empower(dbref player, char *whostr, char *powstr);
extern void do_powers(dbref player, char *arg1);
extern void disp_powers(char *buff, dbref player);

/* game/predicates.c */
extern int  can_see_atr(dbref who, dbref what, ATTR *atr);
extern int  can_set_atr(dbref who, dbref what, ATTR *atr);
extern int  mainroom(dbref player);
extern int  locatable(dbref player, dbref it);
extern int  nearby(dbref player, dbref thing);
extern int  match_plane(dbref player, dbref thing);
extern dbref *find_path(dbref player, dbref target, int max_depth, dbref privs);
extern dbref random_exit(dbref player, dbref loc);
extern char *atr_zone(dbref player, unsigned char num);
extern char *parse_atr_zone(dbref loc, dbref player, unsigned char num, int room);
extern int  is_zone(dbref loc, int flag);
extern int  charge_money(dbref player, int cost, int type);
extern void giveto(dbref player, int pennies);
extern int  get_gender(dbref player);
extern char *gender_subst(dbref player, int type);
extern char *secure_string(char *str);
extern int  ok_attribute_name(char *name);
extern int  ok_filename(char *name);
extern int  ok_name(char *name);
extern int  ok_itemname(char *name);
extern int  ok_player_name(char *name, int type);
extern int  ok_password(char *password);
extern char *possname(char *name);
extern char *pluralize(char *name);
extern char *article(char *name);
extern char *parse_function(dbref privs, dbref player, char *str);
extern void pronoun_substitute(char *buf, dbref privs, dbref player, char *str);
extern void did_it(dbref player, dbref thing, unsigned char Succ, char *emit, unsigned char Osucc, char *oemit, unsigned char Asucc);
extern void ansi_did_it(dbref player, dbref thing, unsigned char atr1, char *msg1, unsigned char atr2, char *msg2, unsigned char atr3);
extern int  hold_player(dbref thing);
extern int  mem_usage(dbref thing);
extern int  playmem_usage(dbref player);

/* game/queue.c */
extern void immediate_que(dbref player, char *cmd, dbref thing);
extern void parse_que(dbref player, char *text, dbref cause);
extern void do_wait(dbref player, char *arg1, char *command, char **argv, char *pure, dbref cause);
extern void flush_queue(int num);
extern void wait_second(void);
extern void do_ps(dbref player, char *arg1, char *arg2);
extern int  halt_object(dbref thing, char *id, int wild);
extern void do_halt(dbref player, char *arg1, char *ncom, char **argv, char *pure, dbref cause);
extern void do_cancel(dbref player, char *obj, char *id);
extern void do_semaphore(dbref player, char *id, char *cmd, char **argv, char *pure, dbref cause);
extern long next_semaid(dbref player, char *id);
extern void semalist(char *buff, dbref player);
extern void save_queues(FILE *f);
extern void load_queues(FILE *f);
extern void restore_queue_quotas(void);

/* game/unparse.c */
extern void list_flags(dbref player);
extern char *unparse_flags(dbref player, dbref thing, int type);
extern int  unparse_color(dbref player, dbref obj, int type);
extern char *unparse_object(dbref player, dbref loc);
extern char *unparse_exitname(dbref player, dbref loc, int alias);
extern char *colorname(dbref player);
extern char *unparse_objheader(dbref player, dbref thing);
extern char *unparse_caption(dbref player, dbref thing);

/* io/console.c */
extern int  get_cols(DESC *d, dbref player);
extern int  get_rows(DESC *d, dbref player);
extern void clear_screen(DESC *d, int sbar);
extern void fade_screen(DESC *d);
extern void do_term(dbref player, char *arg1, char *arg2);
extern void do_cls(dbref player);
extern void do_player_config(dbref player, char *arg1, char *arg2);
extern void eval_term_config(char *buff, dbref player, char *name);
extern void do_console(dbref player, char *name, char *options);
extern char *term_emulation(DESC *d, char *msg, int *len);
extern int  color_code(int prev, char *s);
extern char *unparse_color_code(int color);
extern char *textcolor(int prev, int color);
extern char *rainbowmsg(char *s, int color, int size);
extern char *color_by_cname(char *s, char *cname, int size, int type);
extern char *send_icon(dbref player, int type, char *icon);

/* io/dns.c */
extern int  dns_open(void);
extern void dns_receive(void);
extern void dns_events(void);
extern char *dns_lookup(int ip);
extern int  dns_cache_size(void);
extern char *dns_lookup(int ip);

/* io/file.c */
extern void do_log(dbref player, char *arg1, char *arg2);
extern void mud_log(struct log *l, char *fmt, ...);
extern void lperror(char *file, int line, char *header);
extern void do_motd(dbref player);
extern char *ftext(char *buf, int len, FILE *f);
extern void do_text(dbref player, char *arg1, char *arg2, char **argv);
extern void showlist(dbref player, struct textlist *dirlist, int colsize);
extern char *read_file_line(char *name, int line);
extern void display_textfile(DESC *d, char *filename);
extern void display_textblock(dbref player, DESC *d, char *filename, char *pattern);

/* io/html.c */
extern char *expand_url(char *t);
extern char *parse_url(char *t);
extern int  html_request(DESC *d, char *msg);

/* io/ident.c */
extern void get_ident(DESC *d);
extern int  process_ident(DESC *d, char *msg);

/* io/mail.c */
extern int  mailfile_size(void);
extern int  mail_stats(dbref player, int type);
extern void mail_count(dbref player);
extern int  delete_message(dbref player, int msg);
extern void do_mail(dbref player, char *cmd, char *text);
extern int  service_smtp(DESC *d, char *cmd);
extern int  smtp_data_handler(DESC *d, char *text);

/* io/net.c */
extern void restart_game(void);
extern int  check_executable(void);
extern DESC *initializesock(int s);
extern int  init_connect(DESC *d);
extern int  forbidden_site(DESC *d, int class);
extern int  process_input(DESC *d);
extern void process_input_internal(DESC *d, unsigned char *buf, int len);
extern int  process_output(DESC *d, int poll);
extern void queue_output(DESC *d, int queue, void *msg, int len);
extern void output(DESC *d, char *msg);
extern void output2(DESC *d, char *msg);
extern void output_binary(DESC *d, unsigned char *msg, int len);
extern void flush_io(DESC *d, int index);
extern void do_flush(dbref player, char *arg1, char *arg2);

/* io/server.c */
extern void update_io(void);
extern void do_passwd(dbref player, char *name);
extern void login_procedure(DESC *d);
extern void do_paste(dbref player, char *name);
extern void do_input(dbref player, char *arg1, char *arg2, char **argv);
extern void do_terminate(dbref player, char *obj, char *arg2, char **argv, char *pure, dbref cause);
extern char *conc_title(DESC *d);
extern void do_ctrace(dbref player);
extern int  service_conc(DESC *d, char *msg, int len);

/* io/user.c */
extern void do_who(dbref player, char *flags, char *names);
extern void announce_disconnect(dbref player);
extern void global_trigger(dbref who, unsigned char acmd, int trig);
extern void room_trigger(dbref who, unsigned char acmd);
extern void broadcast(char *msg);
extern void check_timeout(DESC *d);
extern int  isbusy(dbref player);

/* prog/boolexp.c */
extern char *process_lock(dbref player, dbref object, char *arg);
extern int  eval_boolexp(dbref player, dbref object, char *key);

/* prog/ctrl.c */
extern void do_switch(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_foreach(dbref player, char *list, char *command, char **argv, char *pure, dbref cause);
extern void do_iterate(dbref player, char *pattern, char *arg2, char **argv, char *pure, dbref cause);
extern void do_trigger(dbref player, char *object, char *arg2, char **argv, char *pure, dbref cause);
extern void trigger_attr(dbref thing, dbref cause, int obj, unsigned char num);
extern void trigger_all(unsigned char attr);
extern void do_force(dbref player, char *what, char *command, char **argv, char *pure, dbref cause);
extern void instant_force(dbref player, dbref thing, char *command, dbref cause);
extern void do_redirect(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_time(dbref player, char *arg1, char *arg2, char **argv, char *pure, dbref cause);
extern void do_misc(dbref player);
extern void do_use(dbref player, char *arg1, char *arg2, char **args);
extern void do_area_search(dbref player, char *arg1);

/* prog/eval.c */
extern void list_functions(dbref player);
extern int  function(char **str, char *buff, dbref privs, dbref doer);
extern void evaluate(char **str, char *buff, dbref privs, dbref doer, char type);

/* prog/hash.c */
extern hash *make_hash_table(void *entry, int total, int size);
extern void *lookup_hash(hash *tab, char *name);
extern hash *create_hash(void);
extern void add_hash(hash *tab, void *entry);
extern void del_hash(hash *tab, char *name);

/* mare/config.c */
extern void list_config(dbref player, char *arg1);
extern void do_config(dbref player, char *name, char *value);
extern void get_config(dbref player, char *buff, char *name);
extern int  load_config(FILE *f);
extern void save_config(FILE *f);

/* mare/crypt.c */
extern quad des_crypt(char *key, char *salt);
extern char *md5_crypt(char *pw, char *salt, int len);
extern char *crypt(const char *key, const char *salt);
extern char *md5_hash(char *pw, int salt);
extern int  compute_crc(unsigned char *s);

/* mare/rand.c */
extern void mt_srand(int seed);
extern int  mt_rand(void);
extern void mt_repeatable_rand(char *buff, int modulo, int seed, int n, int count);

/* mare/stats.c */
extern char *localhost(void);
extern void do_version(dbref player);
extern void do_uptime(dbref player);
extern void do_memory(dbref player, char *arg1);
extern void init_dbstat(void);
extern void update_accounting(void);
extern void save_records(FILE *f, int rule);
extern void do_info(dbref player, char *type);
extern void do_list(dbref player, char *arg1, char *arg2);
extern void do_dbtop(dbref player, char *arg1, char *arg2);

/* mare/timer.c */
extern quad get_time(void);
extern quad get_cputime(void);
extern void update_cpu_percent(void);
extern void dispatch(void);

/* mare/utils.c */
extern char *wtime(unsigned long td, int typ);
extern char *mkdate(char *str);
extern char *filter_date(char *string);
extern char *strtime(long tt);
extern void timefmt(char *buf, char *fmt, long tt, int usec);
extern void do_tzone(dbref player, char *arg1);
extern char *mktm(dbref thing, long tt);
extern long get_date(char *str, dbref thing, long epoch);
extern char *time_format(int format, long dt);
extern char *grab_time(dbref player);
extern double atof2(const char *str);
extern quad builtin_atoll(const char *buf);
extern void *builtin_memmove(void *dest, void *src, int len);
extern double degrees(double x, double y);
extern char *print_float(char *buf, double val);
extern int  string_prefix(const char *string, const char *prefix);
extern void trim_spaces(char **str);
extern char *parse_up(char **str, int delimit);
extern char *parse_perfect(char **str, int del);
extern char *ansi_parse_up(char **str, int delimit);
extern char *strspc(char **s);
extern char *center(char *str, int len, char fill);
extern char *eval_justify(char **str, int width);
extern int  edit_string(char *dest, char *string, char *old, char *new);
extern char *stralloc(char *str);
