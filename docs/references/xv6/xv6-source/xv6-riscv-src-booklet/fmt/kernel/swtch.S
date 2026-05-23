2900 # Context switch
2901 #
2902 #   void swtch(struct context *old, struct context *new);
2903 #
2904 # Save current registers in old. Load from new.
2905 
2906 
2907 .globl swtch
2908 swtch:
2909         sd ra, 0(a0)
2910         sd sp, 8(a0)
2911         sd s0, 16(a0)
2912         sd s1, 24(a0)
2913         sd s2, 32(a0)
2914         sd s3, 40(a0)
2915         sd s4, 48(a0)
2916         sd s5, 56(a0)
2917         sd s6, 64(a0)
2918         sd s7, 72(a0)
2919         sd s8, 80(a0)
2920         sd s9, 88(a0)
2921         sd s10, 96(a0)
2922         sd s11, 104(a0)
2923 
2924         ld ra, 0(a1)
2925         ld sp, 8(a1)
2926         ld s0, 16(a1)
2927         ld s1, 24(a1)
2928         ld s2, 32(a1)
2929         ld s3, 40(a1)
2930         ld s4, 48(a1)
2931         ld s5, 56(a1)
2932         ld s6, 64(a1)
2933         ld s7, 72(a1)
2934         ld s8, 80(a1)
2935         ld s9, 88(a1)
2936         ld s10, 96(a1)
2937         ld s11, 104(a1)
2938 
2939         ret
2940 
2941 
2942 
2943 
2944 
2945 
2946 
2947 
2948 
2949 
