#!/usr/bin/perl

use POSIX;

my $dckeysymsfile = "dckeysyms.h";
my %dcsyms = ();

open DCKB, "<$dckeysymsfile" || die "Unable to open keysym file $dckeysymsfile";
while(<DCKB>) {
    if( /^#define\s+DCKB_([^ ]*)/ ) {
        $dcsyms{$1} = "DCKB_$1";
    }
}

my %hash = ();
my %rhash = ();
my $name = shift();
while(<ARGV>) {
    my ($val, $sym) = split /\s+/;
    $ival = POSIX::strtol($val,0);
    $hash{$ival} = $sym;
    $rhash{$sym} = $ival;
}

print "/**\n * $name keyboard map autogenerated by genkeymap.pl\n */\n\n";

print "const gchar *${name}_keysyms_by_keycode[128] = { ";
for( $i=0; $i < 128; $i++ ) {
    if( $i != 0 ) { print ", "; }
    if( $hash{$i} ) {
        print "\"$hash{$i}\"";
    } else {
        print "NULL";
    }
}
print "};\n\n";

print "const uint16_t ${name}_keycode_to_dckeysym[128] = { ";
for( $i=0; $i<128; $i++ ) {
    if( $i != 0 ) { print ", "; }
    if( $hash{$i} && $dcsyms{$hash{$i}} ) {
        print $dcsyms{$hash{$i}};
    } else {
        print "DCKB_NONE";
    }
}
print "};\n\n";

my @keys = sort {uc($a) cmp uc($b)} keys %rhash;
print "#define ${name}_keysym_count " . ($#keys+1) . "\n";
print "struct ${name}_keymap_struct {\n    const gchar *name;\n    uint16_t keycode;\n};\n\n";
print "struct ${name}_keymap_struct ${name}_keysyms[] = { ";
foreach my $keysym (@keys) {
    print "{\"$keysym\", $rhash{$keysym} }, ";
}
print "{NULL,-1} };\n\n";
