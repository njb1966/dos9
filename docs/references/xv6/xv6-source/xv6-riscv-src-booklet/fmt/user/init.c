7750 // init: The initial user-level program
7751 
7752 #include "kernel/types.h"
7753 #include "kernel/stat.h"
7754 #include "kernel/spinlock.h"
7755 #include "kernel/sleeplock.h"
7756 #include "kernel/fs.h"
7757 #include "kernel/file.h"
7758 #include "user/user.h"
7759 #include "kernel/fcntl.h"
7760 
7761 char *argv[] = { "sh", 0 };
7762 
7763 int
7764 main(void)
7765 {
7766   int pid, wpid;
7767 
7768   if(open("console", O_RDWR) < 0){
7769     mknod("console", CONSOLE, 0);
7770     open("console", O_RDWR);
7771   }
7772   dup(0);  // stdout
7773   dup(0);  // stderr
7774 
7775   for(;;){
7776     printf("init: starting sh\n");
7777     pid = fork();
7778     if(pid < 0){
7779       printf("init: fork failed\n");
7780       exit(1);
7781     }
7782     if(pid == 0){
7783       exec("sh", argv);
7784       printf("init: exec sh failed\n");
7785       exit(1);
7786     }
7787 
7788     for(;;){
7789       // this call to wait() returns if the shell exits,
7790       // or if a parentless process exits.
7791       wpid = wait((int *) 0);
7792       if(wpid == pid){
7793         // the shell exited; restart it.
7794         break;
7795       } else if(wpid < 0){
7796         printf("init: wait returned an error\n");
7797         exit(1);
7798       } else {
7799         // it was a parentless process; do nothing.
7800       }
7801     }
7802   }
7803 }
7804 
7805 
7806 
7807 
7808 
7809 
7810 
7811 
7812 
7813 
7814 
7815 
7816 
7817 
7818 
7819 
7820 
7821 
7822 
7823 
7824 
7825 
7826 
7827 
7828 
7829 
7830 
7831 
7832 
7833 
7834 
7835 
7836 
7837 
7838 
7839 
7840 
7841 
7842 
7843 
7844 
7845 
7846 
7847 
7848 
7849 
