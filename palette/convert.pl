#!/usr/bin/perl
use warnings;
use strict;
use autodie;
use Data::Dumper;

my @EGApalettes;    #contains (up to) 14 "palettes" with 16 colors each, to be used by i_setpalette
my @Carrays; 		#contains (up to) 14 arrays that map 0-255 to 0-16 (VGA palette to EGA palette)

print "const byte EGApalettes[14][16] = { ";
for my $i (1..14) {
	if (-f "$i.csv") {
		open my $fh, "<", "$i.csv";
		my $coloridx = 0;
		my $newcolidx = 0;
		my %colors; #contains the unique colors seen until now, with the new id
		$Carrays[$i] = ();
		while (my $line = <$fh>) {
			chomp $line;
			if (defined $colors{$line}) {
				$Carrays[$i][$coloridx]=$colors{$line};
			}
			else {
				$colors{$line}=$newcolidx;
				$EGApalettes[$i][$newcolidx]=$line;
				$Carrays[$i][$coloridx]=$newcolidx;
				$newcolidx++;			
			}
			$coloridx++;
		}
		#if curr palette doesn't have 16 colors in total, add random colors
		while (scalar(@{$EGApalettes[$i]})<16) {
			push @{$EGApalettes[$i]}, "0, 0, 0";
		}
	}
	else {
		do {
			$i = $i-1;
		} while (!defined $EGApalettes[$i]);
	}	
	print "{ ";
	print join(", ",@{$EGApalettes[$i]});
	print " },\n";
}
print "};\n";

print "const byte newcolor[14][256] = { ";
for my $i (1..14) {
	while (!defined $Carrays[$i]) {
		$i = $i-1;
	};
	print "{ ";
	print join(", ",@{$Carrays[$i]});
	print "},\n";
}
print "};\n";


