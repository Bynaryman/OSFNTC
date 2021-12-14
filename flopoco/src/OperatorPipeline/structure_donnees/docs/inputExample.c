uint(w) n1, n3;
sint(w) n2, n4;
float(wE, WF) f;
ufix(m, l) fp1;
sfix(m, l) fp2;

n1=uint(w)( f+float(fp1) );
n2=sint(w)( f*float(fp2) );
n3= uint(w)( exp( f/float(fp1) );
n4=sint(w)( log(exp( f/float(fp2) ) ) );

return n1, n2, n3, n4;
