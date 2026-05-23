6750 #include "types.h"
6751 
6752 void*
6753 memset(void *dst, int c, uint n)
6754 {
6755   char *cdst = (char *) dst;
6756   int i;
6757   for(i = 0; i < n; i++){
6758     cdst[i] = c;
6759   }
6760   return dst;
6761 }
6762 
6763 int
6764 memcmp(const void *v1, const void *v2, uint n)
6765 {
6766   const uchar *s1, *s2;
6767 
6768   s1 = v1;
6769   s2 = v2;
6770   while(n-- > 0){
6771     if(*s1 != *s2)
6772       return *s1 - *s2;
6773     s1++, s2++;
6774   }
6775 
6776   return 0;
6777 }
6778 
6779 void*
6780 memmove(void *dst, const void *src, uint n)
6781 {
6782   const char *s;
6783   char *d;
6784 
6785   if(n == 0)
6786     return dst;
6787 
6788   s = src;
6789   d = dst;
6790   if(s < d && s + n > d){
6791     s += n;
6792     d += n;
6793     while(n-- > 0)
6794       *--d = *--s;
6795   } else
6796     while(n-- > 0)
6797       *d++ = *s++;
6798 
6799 
6800   return dst;
6801 }
6802 
6803 // memcpy exists to placate GCC.  Use memmove.
6804 void*
6805 memcpy(void *dst, const void *src, uint n)
6806 {
6807   return memmove(dst, src, n);
6808 }
6809 
6810 int
6811 strncmp(const char *p, const char *q, uint n)
6812 {
6813   while(n > 0 && *p && *p == *q)
6814     n--, p++, q++;
6815   if(n == 0)
6816     return 0;
6817   return (uchar)*p - (uchar)*q;
6818 }
6819 
6820 char*
6821 strncpy(char *s, const char *t, int n)
6822 {
6823   char *os;
6824 
6825   os = s;
6826   while(n-- > 0 && (*s++ = *t++) != 0)
6827     ;
6828   while(n-- > 0)
6829     *s++ = 0;
6830   return os;
6831 }
6832 
6833 // Like strncpy but guaranteed to NUL-terminate.
6834 char*
6835 safestrcpy(char *s, const char *t, int n)
6836 {
6837   char *os;
6838 
6839   os = s;
6840   if(n <= 0)
6841     return os;
6842   while(--n > 0 && (*s++ = *t++) != 0)
6843     ;
6844   *s = 0;
6845   return os;
6846 }
6847 
6848 
6849 
6850 int
6851 strlen(const char *s)
6852 {
6853   int n;
6854 
6855   for(n = 0; s[n]; n++)
6856     ;
6857   return n;
6858 }
6859 
6860 
6861 
6862 
6863 
6864 
6865 
6866 
6867 
6868 
6869 
6870 
6871 
6872 
6873 
6874 
6875 
6876 
6877 
6878 
6879 
6880 
6881 
6882 
6883 
6884 
6885 
6886 
6887 
6888 
6889 
6890 
6891 
6892 
6893 
6894 
6895 
6896 
6897 
6898 
6899 
