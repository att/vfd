# master makefile for inclusion

# install transfig package to get fig2dev
%.eps: %.fig
    fig2dev -L eps ${prereq} ${target}

%.png: %.fig
    fig2dev -L png ${prereq} ${target}

%.png: %.fig
    fig2dev -L png ${prereq} ${target}


%_slides :  %_slides.xfm
	pfm -g 8ix11i ${prereq%% *} $target.ps

%.ps :  %.xfm
	pfm ${prereq%% *} $target 

%.html :  %.xfm
	hfm ${prereq%% *} $target 

%.pdf : %.ps
	gs -I/usr/local/share/ghostscript/fonts -dBATCH -dNOPAUSE -sDEVICE=pdfwrite -sOutputFile=$target ${prereq% *}

%.txt : %.xfm
	tfm ${prereq%% *} $target 

