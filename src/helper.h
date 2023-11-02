
#define HOW_LONG(name,func) \
                        if(true){\
                        long __time1=ESP.getCycleCount();\
                        func;\
                        long __time2=ESP.getCycleCount()-__time1;\ 
                        printf("The function *** %s *** took %.2f ms or %.2f fps\n",name,(float)__time2/24000,(float) 240000000/__time2);\
                        }