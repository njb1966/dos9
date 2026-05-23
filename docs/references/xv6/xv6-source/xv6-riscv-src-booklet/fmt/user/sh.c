7850 // Shell.
7851 
7852 #include "kernel/types.h"
7853 #include "user/user.h"
7854 #include "kernel/fcntl.h"
7855 
7856 // Parsed command representation
7857 #define EXEC  1
7858 #define REDIR 2
7859 #define PIPE  3
7860 #define LIST  4
7861 #define BACK  5
7862 
7863 #define MAXARGS 10
7864 
7865 struct cmd {
7866   int type;
7867 };
7868 
7869 struct execcmd {
7870   int type;
7871   char *argv[MAXARGS];
7872   char *eargv[MAXARGS];
7873 };
7874 
7875 struct redircmd {
7876   int type;
7877   struct cmd *cmd;
7878   char *file;
7879   char *efile;
7880   int mode;
7881   int fd;
7882 };
7883 
7884 struct pipecmd {
7885   int type;
7886   struct cmd *left;
7887   struct cmd *right;
7888 };
7889 
7890 struct listcmd {
7891   int type;
7892   struct cmd *left;
7893   struct cmd *right;
7894 };
7895 
7896 struct backcmd {
7897   int type;
7898   struct cmd *cmd;
7899 };
7900 int fork1(void);  // Fork but panics on failure.
7901 void panic(char*);
7902 struct cmd *parsecmd(char*);
7903 void runcmd(struct cmd*) __attribute__((noreturn));
7904 
7905 // Execute cmd.  Never returns.
7906 void
7907 runcmd(struct cmd *cmd)
7908 {
7909   int p[2];
7910   struct backcmd *bcmd;
7911   struct execcmd *ecmd;
7912   struct listcmd *lcmd;
7913   struct pipecmd *pcmd;
7914   struct redircmd *rcmd;
7915 
7916   if(cmd == 0)
7917     exit(1);
7918 
7919   switch(cmd->type){
7920   default:
7921     panic("runcmd");
7922 
7923   case EXEC:
7924     ecmd = (struct execcmd*)cmd;
7925     if(ecmd->argv[0] == 0)
7926       exit(1);
7927     exec(ecmd->argv[0], ecmd->argv);
7928     fprintf(2, "exec %s failed\n", ecmd->argv[0]);
7929     break;
7930 
7931   case REDIR:
7932     rcmd = (struct redircmd*)cmd;
7933     close(rcmd->fd);
7934     if(open(rcmd->file, rcmd->mode) < 0){
7935       fprintf(2, "open %s failed\n", rcmd->file);
7936       exit(1);
7937     }
7938     runcmd(rcmd->cmd);
7939     break;
7940 
7941   case LIST:
7942     lcmd = (struct listcmd*)cmd;
7943     if(fork1() == 0)
7944       runcmd(lcmd->left);
7945     wait(0);
7946     runcmd(lcmd->right);
7947     break;
7948 
7949 
7950   case PIPE:
7951     pcmd = (struct pipecmd*)cmd;
7952     if(pipe(p) < 0)
7953       panic("pipe");
7954     if(fork1() == 0){
7955       close(1);
7956       dup(p[1]);
7957       close(p[0]);
7958       close(p[1]);
7959       runcmd(pcmd->left);
7960     }
7961     if(fork1() == 0){
7962       close(0);
7963       dup(p[0]);
7964       close(p[0]);
7965       close(p[1]);
7966       runcmd(pcmd->right);
7967     }
7968     close(p[0]);
7969     close(p[1]);
7970     wait(0);
7971     wait(0);
7972     break;
7973 
7974   case BACK:
7975     bcmd = (struct backcmd*)cmd;
7976     if(fork1() == 0)
7977       runcmd(bcmd->cmd);
7978     break;
7979   }
7980   exit(0);
7981 }
7982 
7983 int
7984 getcmd(char *buf, int nbuf)
7985 {
7986   write(2, "$ ", 2);
7987   memset(buf, 0, nbuf);
7988   gets(buf, nbuf);
7989   if(buf[0] == 0) // EOF
7990     return -1;
7991   return 0;
7992 }
7993 
7994 
7995 
7996 
7997 
7998 
7999 
8000 int
8001 main(void)
8002 {
8003   static char buf[100];
8004   int fd;
8005 
8006   // Ensure that three file descriptors are open.
8007   while((fd = open("console", O_RDWR)) >= 0){
8008     if(fd >= 3){
8009       close(fd);
8010       break;
8011     }
8012   }
8013 
8014   // Read and run input commands.
8015   while(getcmd(buf, sizeof(buf)) >= 0){
8016     char *cmd = buf;
8017     while (*cmd == ' ' || *cmd == '\t')
8018       cmd++;
8019     if (*cmd == '\n') // is a blank command
8020       continue;
8021     if(cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' '){
8022       // Chdir must be called by the parent, not the child.
8023       cmd[strlen(cmd)-1] = 0;  // chop \n
8024       if(chdir(cmd+3) < 0)
8025         fprintf(2, "cannot cd %s\n", cmd+3);
8026     } else {
8027       if(fork1() == 0)
8028         runcmd(parsecmd(cmd));
8029       wait(0);
8030     }
8031   }
8032   exit(0);
8033 }
8034 
8035 void
8036 panic(char *s)
8037 {
8038   fprintf(2, "%s\n", s);
8039   exit(1);
8040 }
8041 
8042 
8043 
8044 
8045 
8046 
8047 
8048 
8049 
8050 int
8051 fork1(void)
8052 {
8053   int pid;
8054 
8055   pid = fork();
8056   if(pid == -1)
8057     panic("fork");
8058   return pid;
8059 }
8060 
8061 
8062 
8063 
8064 
8065 
8066 
8067 
8068 
8069 
8070 
8071 
8072 
8073 
8074 
8075 
8076 
8077 
8078 
8079 
8080 
8081 
8082 
8083 
8084 
8085 
8086 
8087 
8088 
8089 
8090 
8091 
8092 
8093 
8094 
8095 
8096 
8097 
8098 
8099 
8100 // Constructors
8101 
8102 struct cmd*
8103 execcmd(void)
8104 {
8105   struct execcmd *cmd;
8106 
8107   cmd = malloc(sizeof(*cmd));
8108   memset(cmd, 0, sizeof(*cmd));
8109   cmd->type = EXEC;
8110   return (struct cmd*)cmd;
8111 }
8112 
8113 struct cmd*
8114 redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
8115 {
8116   struct redircmd *cmd;
8117 
8118   cmd = malloc(sizeof(*cmd));
8119   memset(cmd, 0, sizeof(*cmd));
8120   cmd->type = REDIR;
8121   cmd->cmd = subcmd;
8122   cmd->file = file;
8123   cmd->efile = efile;
8124   cmd->mode = mode;
8125   cmd->fd = fd;
8126   return (struct cmd*)cmd;
8127 }
8128 
8129 struct cmd*
8130 pipecmd(struct cmd *left, struct cmd *right)
8131 {
8132   struct pipecmd *cmd;
8133 
8134   cmd = malloc(sizeof(*cmd));
8135   memset(cmd, 0, sizeof(*cmd));
8136   cmd->type = PIPE;
8137   cmd->left = left;
8138   cmd->right = right;
8139   return (struct cmd*)cmd;
8140 }
8141 
8142 
8143 
8144 
8145 
8146 
8147 
8148 
8149 
8150 struct cmd*
8151 listcmd(struct cmd *left, struct cmd *right)
8152 {
8153   struct listcmd *cmd;
8154 
8155   cmd = malloc(sizeof(*cmd));
8156   memset(cmd, 0, sizeof(*cmd));
8157   cmd->type = LIST;
8158   cmd->left = left;
8159   cmd->right = right;
8160   return (struct cmd*)cmd;
8161 }
8162 
8163 struct cmd*
8164 backcmd(struct cmd *subcmd)
8165 {
8166   struct backcmd *cmd;
8167 
8168   cmd = malloc(sizeof(*cmd));
8169   memset(cmd, 0, sizeof(*cmd));
8170   cmd->type = BACK;
8171   cmd->cmd = subcmd;
8172   return (struct cmd*)cmd;
8173 }
8174 
8175 
8176 
8177 
8178 
8179 
8180 
8181 
8182 
8183 
8184 
8185 
8186 
8187 
8188 
8189 
8190 
8191 
8192 
8193 
8194 
8195 
8196 
8197 
8198 
8199 
8200 // Parsing
8201 
8202 char whitespace[] = " \t\r\n\v";
8203 char symbols[] = "<|>&;()";
8204 
8205 int
8206 gettoken(char **ps, char *es, char **q, char **eq)
8207 {
8208   char *s;
8209   int ret;
8210 
8211   s = *ps;
8212   while(s < es && strchr(whitespace, *s))
8213     s++;
8214   if(q)
8215     *q = s;
8216   ret = *s;
8217   switch(*s){
8218   case 0:
8219     break;
8220   case '|':
8221   case '(':
8222   case ')':
8223   case ';':
8224   case '&':
8225   case '<':
8226     s++;
8227     break;
8228   case '>':
8229     s++;
8230     if(*s == '>'){
8231       ret = '+';
8232       s++;
8233     }
8234     break;
8235   default:
8236     ret = 'a';
8237     while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
8238       s++;
8239     break;
8240   }
8241   if(eq)
8242     *eq = s;
8243 
8244   while(s < es && strchr(whitespace, *s))
8245     s++;
8246   *ps = s;
8247   return ret;
8248 }
8249 
8250 int
8251 peek(char **ps, char *es, char *toks)
8252 {
8253   char *s;
8254 
8255   s = *ps;
8256   while(s < es && strchr(whitespace, *s))
8257     s++;
8258   *ps = s;
8259   return *s && strchr(toks, *s);
8260 }
8261 
8262 struct cmd *parseline(char**, char*);
8263 struct cmd *parsepipe(char**, char*);
8264 struct cmd *parseexec(char**, char*);
8265 struct cmd *nulterminate(struct cmd*);
8266 
8267 struct cmd*
8268 parsecmd(char *s)
8269 {
8270   char *es;
8271   struct cmd *cmd;
8272 
8273   es = s + strlen(s);
8274   cmd = parseline(&s, es);
8275   peek(&s, es, "");
8276   if(s != es){
8277     fprintf(2, "leftovers: %s\n", s);
8278     panic("syntax");
8279   }
8280   nulterminate(cmd);
8281   return cmd;
8282 }
8283 
8284 struct cmd*
8285 parseline(char **ps, char *es)
8286 {
8287   struct cmd *cmd;
8288 
8289   cmd = parsepipe(ps, es);
8290   while(peek(ps, es, "&")){
8291     gettoken(ps, es, 0, 0);
8292     cmd = backcmd(cmd);
8293   }
8294   if(peek(ps, es, ";")){
8295     gettoken(ps, es, 0, 0);
8296     cmd = listcmd(cmd, parseline(ps, es));
8297   }
8298   return cmd;
8299 }
8300 struct cmd*
8301 parsepipe(char **ps, char *es)
8302 {
8303   struct cmd *cmd;
8304 
8305   cmd = parseexec(ps, es);
8306   if(peek(ps, es, "|")){
8307     gettoken(ps, es, 0, 0);
8308     cmd = pipecmd(cmd, parsepipe(ps, es));
8309   }
8310   return cmd;
8311 }
8312 
8313 struct cmd*
8314 parseredirs(struct cmd *cmd, char **ps, char *es)
8315 {
8316   int tok;
8317   char *q, *eq;
8318 
8319   while(peek(ps, es, "<>")){
8320     tok = gettoken(ps, es, 0, 0);
8321     if(gettoken(ps, es, &q, &eq) != 'a')
8322       panic("missing file for redirection");
8323     switch(tok){
8324     case '<':
8325       cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
8326       break;
8327     case '>':
8328       cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
8329       break;
8330     case '+':  // >>
8331       cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
8332       break;
8333     }
8334   }
8335   return cmd;
8336 }
8337 
8338 
8339 
8340 
8341 
8342 
8343 
8344 
8345 
8346 
8347 
8348 
8349 
8350 struct cmd*
8351 parseblock(char **ps, char *es)
8352 {
8353   struct cmd *cmd;
8354 
8355   if(!peek(ps, es, "("))
8356     panic("parseblock");
8357   gettoken(ps, es, 0, 0);
8358   cmd = parseline(ps, es);
8359   if(!peek(ps, es, ")"))
8360     panic("syntax - missing )");
8361   gettoken(ps, es, 0, 0);
8362   cmd = parseredirs(cmd, ps, es);
8363   return cmd;
8364 }
8365 
8366 struct cmd*
8367 parseexec(char **ps, char *es)
8368 {
8369   char *q, *eq;
8370   int tok, argc;
8371   struct execcmd *cmd;
8372   struct cmd *ret;
8373 
8374   if(peek(ps, es, "("))
8375     return parseblock(ps, es);
8376 
8377   ret = execcmd();
8378   cmd = (struct execcmd*)ret;
8379 
8380   argc = 0;
8381   ret = parseredirs(ret, ps, es);
8382   while(!peek(ps, es, "|)&;")){
8383     if((tok=gettoken(ps, es, &q, &eq)) == 0)
8384       break;
8385     if(tok != 'a')
8386       panic("syntax");
8387     cmd->argv[argc] = q;
8388     cmd->eargv[argc] = eq;
8389     argc++;
8390     if(argc >= MAXARGS)
8391       panic("too many args");
8392     ret = parseredirs(ret, ps, es);
8393   }
8394   cmd->argv[argc] = 0;
8395   cmd->eargv[argc] = 0;
8396   return ret;
8397 }
8398 
8399 
8400 // NUL-terminate all the counted strings.
8401 struct cmd*
8402 nulterminate(struct cmd *cmd)
8403 {
8404   int i;
8405   struct backcmd *bcmd;
8406   struct execcmd *ecmd;
8407   struct listcmd *lcmd;
8408   struct pipecmd *pcmd;
8409   struct redircmd *rcmd;
8410 
8411   if(cmd == 0)
8412     return 0;
8413 
8414   switch(cmd->type){
8415   case EXEC:
8416     ecmd = (struct execcmd*)cmd;
8417     for(i=0; ecmd->argv[i]; i++)
8418       *ecmd->eargv[i] = 0;
8419     break;
8420 
8421   case REDIR:
8422     rcmd = (struct redircmd*)cmd;
8423     nulterminate(rcmd->cmd);
8424     *rcmd->efile = 0;
8425     break;
8426 
8427   case PIPE:
8428     pcmd = (struct pipecmd*)cmd;
8429     nulterminate(pcmd->left);
8430     nulterminate(pcmd->right);
8431     break;
8432 
8433   case LIST:
8434     lcmd = (struct listcmd*)cmd;
8435     nulterminate(lcmd->left);
8436     nulterminate(lcmd->right);
8437     break;
8438 
8439   case BACK:
8440     bcmd = (struct backcmd*)cmd;
8441     nulterminate(bcmd->cmd);
8442     break;
8443   }
8444   return cmd;
8445 }
8446 
8447 
8448 
8449 
