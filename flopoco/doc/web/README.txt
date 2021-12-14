To produce the tgz of a version:
first edit VERSION,
then in bash:
git archive --prefix=flopoco-$(cat VERSION)/ -o flopoco-$(cat VERSION).tgz HEAD
Place it on the forge;
Then edit  flopoco_installation_manual.html using the path created by the forge.
commit and push, and scp it as below.
Finally do a 
git tag -a flopoco-$(cat VERSION) -m"Release!"

git push --tags


To update the web site from an admin account:

The operators.html page is generated from flopoco itself:
./flopoco BuildHTMLDoc

scp   flopoco_installation.html   scm.gforge.inria.fr:/home/groups/flopoco/htdocs/
scp   flopoco_user_manual.html  scm.gforge.inria.fr:/home/groups/flopoco/htdocs/
scp   index.html   scm.gforge.inria.fr:/home/groups/flopoco/htdocs/
scp   operators.html   scm.gforge.inria.fr:/home/groups/flopoco/htdocs/
scp   ReleaseNotes.txt   scm.gforge.inria.fr:/home/groups/flopoco/htdocs/

(etc)





The html bibliography pages are generated using bibtex2html
1/ edit doc/web/bib/flopoco.bib or doc/web/bib/flopoco-users.bib
2/ in the doc/web/bib directory, run:

bibtex2html -t "Publications about FloPoCo" --header "<p><em>If some of your works belong there, please drop a mail to F. de Dinechin with the corresponding bibtex entries</em></p><hr>" -d -r -revkeys flopoco.bib
bibtex2html -t "Publications using FloPoCo" --header "<p><em>If some of your works belong there, please drop a mail to F. de Dinechin with the corresponding bibtex entries</em></p><hr>" -d -r -revkeys flopoco-users.bib

3/ transfer the full bib directory with (from doc/web)
scp -r bib/ scm.gforge.inria.fr:/home/groups/flopoco/htdocs/
