3900 struct buf {
3901   int valid;   // has data been read from disk?
3902   int disk;    // does disk "own" buf?
3903   uint dev;
3904   uint blockno;
3905   struct sleeplock lock;
3906   uint refcnt;
3907   struct buf *prev; // LRU cache list
3908   struct buf *next;
3909   uchar data[BSIZE];
3910 };
3911 
3912 
3913 
3914 
3915 
3916 
3917 
3918 
3919 
3920 
3921 
3922 
3923 
3924 
3925 
3926 
3927 
3928 
3929 
3930 
3931 
3932 
3933 
3934 
3935 
3936 
3937 
3938 
3939 
3940 
3941 
3942 
3943 
3944 
3945 
3946 
3947 
3948 
3949 
