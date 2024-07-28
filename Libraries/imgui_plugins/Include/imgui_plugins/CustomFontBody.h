#include "CustomFont.h"

static const char FONT_ICON_BUFFER_NAME_IGFD[3465 + 1] =
"7])#######IP/b*'/###W),##*Mc##f/5##EI[^Igrdg#H4)=-XNE/1-2JuBC/LcDW->>#+tEn/e_[FHs2$L#6;#-MXv#NDb&2t'=&WuL4Z]#MBJ=jLu&g9(B+AkLM^_sLQaX1D=74_I"
">2D9#jn8*#gn3*C'.;M+/7mwLMoHlLR`U[M&#e>#sLFjI'he<6+RE9%6Fu&#Nlptee+^,MPFQ:v-f:?#0%d<-*%6=Mu&g+M-YZ##0Urt--Aa+MNq;?#-D^w'1knP'D)Uj(f$IP/371^#"
"Qfq@-w@g;-lVjfL$.0/LpuM/Lq4P4/x4E/LmEn-$x4+gL7:#gL-9eAOE3;.MTiJGMxb[+NI=-##Plh3=Vl:;$t:,8.;'niL]]R.qjm$0q:;02qn'j4qX%=9q>bm;qo/3`q(&w_q&R$[^"
"Ha8p&oLZSJet7@B5VbA#OJn>-*ALM#EU(DNrqJfL^)^fL:NH##]v&w32Sl##6`($#:l:$#>xL$#B.`$#5HZD-PY5<-QFC@-.(.m/]6YY#4@uu#f).m/wRUV$vYqr$_YlS.De68%RYlS."
"NmQS%&YlS.Nwmo%wWjfLtQCaWX8f/h@t`xF`lY2(@/<v5*8###Xn@X-jbtM(pSLnuw);GV_u.Dt?ba@tKD1[BR&>uuYg8e$UNc##D####[oC&MbPMG)@f)T/^b%T%S2`^#J:8Y.pV*i("
"T=)?#h-[guT#9iu&](?#A]wG;[Dm]uB*07QK(]qFV=fV$H[`V$#kUK#$8^fLmw@8%P92^uJ98=/+@u2ORh.o`5BOojOps+/$),##[YEt$bcx5#kN:tLmb]s$wkH>#iE(E#VA@r%Mep$-"
"#b?1,G2J1,mQOhuvne.Mv?75/Huiw'6`?$=VC]&,EH*7M:9s%,:7QVQM]X-?^10ip&ExrQF7$##lnr?#&r&t%NEE/2XB+a4?c=?/^0Xp%kH$IM$?YCjwpRX:CNNjL<b7:.rH75/1uO]u"
"npchLY.s%,R=q9Vi53'#)5###+`&gLrfhi'r)#,2L3DP85Z&##OZ'u$BKc8/)$fF4Rq@.*a:S'4$ctM(4QCD3a9uD#=u;9/#@o8%Y,A_8x&`;.:3.U0wAOZ6m('J3=6F2MVE8x,8&B.*"
"EdCO'^e$IM4,u4S-3@2Li/?k'mtf&$ISW1BrsG=%Ol+J&YmTr(2'-?8,K+<A%U/K3YF:q'0;PS7D8uN9$f8h(ZC7%b5=(^#R-Ra&uE9#$7[61*:bWq.ElC&&qMKpRM-Q&u]q8dHb-bZM"
"W.Y+/X16aXc3n0#_)###Tx^##x@PS.kN%##AFDZ#5D-W.CSBp7&4RKu0grk'wPCD3EU@lL/RXD#Z?e*NieZ=7:GUv-_Q3L#;1sB#TekD#3YJ,3VG+G4XX48nw8<cDMa]iuium9)igAE+"
"%]v7/gu%t]:U.g9wCo)*sK#(+M]Oo7hgYDOrbdU)7xO-)V&dN#3BQG)uP/O45FMo$n2/tu-i(UL+0IQ1p`b'OMk8hGn2R.)PNd*7cTPDOjx3R/DUsl&F;*VCGkG&#rCbA#:-eSSZB2.0"
"@,>>#6'PA#EaqH/v;Tv--;o+M&Y7C#v($lLbjb@$RieX-PbU/)9,d]u*l0[.p2rE3*cA/L>%jx-q-U&5YZbA#Dc$a-1F-Z$3(m<-3/A'.X(JfL#OP,M9I&c$3W5D<G.H]FMM'##e2IdF"
"2UkD#O^f;-W'n<-<<oY-PGo,=>&B.*NB^&?'?C&QKvheOm0f6/Da14'GtMnS%@,gL7ifbMWFuS7tw^[On98Y-G)G(/fFl%l*anA#UGZW-]L=r)F6jI-9%.Q4KPUV$:;/87R`I/L7_JM'"
"EUgr6lQ%##4IF<%+<9%#Wu$s$/&3,)#j3^4)$fF4l/^I*vu.l'xx:9/:5Rv$t@0+*v>0QAwc``3Z3f.*QoXV-=Y(?#7c7%-QN+x#s19KHPK4;-<9i[(_Y*5A=../uY,wOYs>r>7o(;v#"
"+Y8h(du-%bew,?8]p&<ANSCn/M.+Auh:;S$gwS=l^XK+,99blAGqW&5$*mtNZ'%lf'Q5w.eNv:%c54[9>c#Q8wY`8At1pgLJ[A[#$_^UunPa^#ha@;-dNo?9Ub4'oO),##UjxE%>dL7#"
"%iIf$IZc8/Y^Tv-`fWI)=0..M,+*T/''h^#+ZXl(ARcWqIJ^I*>9(,)joVb3(=wsT$P:'7b?uJ(U0O9%l4jiKr3CVdr(X1)nPGMT^,RW%0<,A#/@PD4I2?[CsQj8.acRL(p(n;%Zm21d"
"?R#N#;DOjLA%g+M)K$##.JE/LF7$##sPT29'v3WS^>@#%LI@X-F`A/L8Sb1Bm79+08&,w/hrVB#$K.q.%H3'5DdDs)>'VHm:`Bv$IE0K)jC,29'55d3g6Y-?:oSfL-LK/L@i6o7:fKb@"
"'x_5/Q1wPMIR0cMmbR`aRLb>->20_-R3n0#bsn`$bA%%#N,>>#MhSM'xe'eZ<D,c4XHFg$.%eX-Si(?#rO-986iG,*<C2=%Ib7o[(f/dtZ9R%-Uwe'&c:]#.S]M4uNDKm6-_r>#s'C@-"
"NJ6/:.N,VH+QB],H0exO-@hB#P%19:Kxu;$OcB1PB7i;-):Im/9(V$#lpB'#Sf.b/0Zc8/onX@$?,vG*OpAgWuhYV-3R-b[8lU^$B)t1)O$pH*adP/(HZ%12ARj-$4Yk,246H^tAj*.)"
"W+YF'il`@$$F/a*/PDh'J3no[Uo5o7DAS?cvJm%$+99T([t&2BJODL5gctW#T3H]u[u)<-&qVW#5M7L(VhQgu8w%?Ak5h,+@@s/40rACuQ5<;$.S;:%vVOV-D'%29M#_B#4ZCD3S)&T/"
"29orNwJ1N(k+;E4k&B.*-4h0,__LP8YxZ#6<-C]$N>.U%')^Y,SEW-$$;(v#B%C7&D$KH2;YvAupvh4L]/8$$n=x_uFE&,&0;s9)1rTk+>QTLVtL@A#+sUB#&Da-$B&G:&[`IH)Ycm20"
"l(.GV93dr/8344ka]N1B4L$Z$J+1q/CCg;,.$###G%+n-WN3L#CoRe*5nOJ(h5.)*jr/+*WoO1YgK$)*.:^^'ZXLxLGF;&50).:2H8>>#mHMZu%JR=-aL7=l,;Bv$&aR&#05+v/Pke%#"
"qvK'#&W<W9+7^;.-5'w%&-2K(=5MB#57qHM5=uM(h7r*4w=D;$+dfF4/bbX$W9ZdN_w-tLquFL#[/;Zu[$^iBTvJs4Po7/Lb2=CLIw[%=En$SV8J$1,(dWIL=#gr/agM%O1Q;>5@R<=$"
"/rVB#kwF>#euita$X82LhuA&G@W:3t0ss48]`a-Zups+;(=rhZ;ktD#c9OA#axJ+*?7%s$DXI5/CKU:%eQ+,2=C587Rg;E4Z3f.*?ulW-tYqw0`EmS/si+C#[<;S&mInS%FAov#Fu[E+"
"*L'O'GWuN'+)U40`a5k'sA;)*RIRF%Tw/Z--1=e?5;1X:;vPk&CdNjL*(KJ1/,TV-G(^S*v14gL#8,,M]@aZ#ZcH@bF4iV$o8CkoW####";

