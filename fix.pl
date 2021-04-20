#!/usr/bin/perl
use warnings;
use strict;
use autodie;
use File::Slurp;

my @cfiles = glob("*.c");

for my $file (@cfiles) {
	if (if -f "$file.old") {
		warn "$file.old exists, skipping $file...\n";
		next;
	}
	my @output = `./csourceparser.pl -d -f $file`;
	my @cfile = read_file($file);
	my @functions;
	my %declars;
	my $is_function = 1;
	for my $line (@output) {
		chomp $line;
		$is_function = 0 if $line =~ /Declarations:/;
		next unless $line =~ /;/;
		next unless $line =~ /[a-z0-9]/i;
		if ($is_function) {
			push @functions, $line;
		}
		else {
			$line =~ s/^\s+//;
			my @fields = split /\s+/, $line;
			$declars{$fields[0]} = 1;
		}
	}
	my $lasti = 0;
	for my $function (@functions) {
		chomp $function;
		my ($returntype, $name, @params) = split /\s+/, $function;
		my $found = 0;
		for (my $i = $lasti; $i < scalar(@cfile); $i++) {
			my $line = $cfile[$i];
			next if $line !~ /\Q$name\E/;
			if ($line !~ /\Q$returntype\E\s*\Q$name\E/) {
				my $prevline = $cfile[$i-1];
				chomp $prevline;
				$prevline =~ s/^\s+//;
				$prevline =~ s/\/\*.*?\*\///g;
				$prevline =~ s/\s+$//;				
				next if $prevline =~ /;/;
				my $curline = "$prevline $line";
				next if ($curline !~ /\Q$returntype\E\s*\Q$name\E/);
			}
			#now jump forward until the first open {
			my $j = $i+1;
			while ($cfile[$j] !~ /\{/) {
				$j++;
			}
			$j++;
			#now jump forward if you find comments or declarations
			#then add the new debug print
			for ( ; $j < scalar(@cfile) ; $j++) {
				my $line = $cfile[$j];
				next if $line =~ /^\s*$/;
				$line =~ s/^\s*//;
				if ($line =~ /^\/\*/) {
					while ($cfile[$j] !~ /\*\//) {
						$j++;
					}
					$j++;
					$line = $cfile[$j]
				}
			   my @fields = split /\s|\*/, $line;
			   next if $declars{$fields[0]};
			   #otherwise we can add it here!
			   splice(@cfile,$j,0,"fprintf(outdbg, \"$name\\n\");\n");
			   $found++;
			   $lasti = $j;
			   last;
			}	
		}
		if ($found==0) {
			warn "couldn't resolve function $function in file $file\n";
		}
	}
	rename $file, "$file.old";
	write_file($file, @cfile);
}