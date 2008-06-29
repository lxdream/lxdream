#!/usr/bin/perl
# Script for OS X to copy all non-core library dependencies into the bundle, and fixup the lib naming
# Run after the executable is already in the bundle.

if( $#ARGV <= 0 ) {
   die( "Usage: bundlelibs.pl <target-binary> <target-lib-path>\n" );
}

my $BINARY=shift();
my $TARGETDIR=shift();
my $OTOOL="otool";
my $NTOOL="install_name_tool";

mkdir $TARGETDIR;

my %done=();
my @worklist = ($BINARY);

while( $#worklist >= 0 ) {
    my $target = shift @worklist;
    $done{$target} = 2;
    
    open FH, "$OTOOL -L $target|" || die "Unable to run otool";
    $skip = <FH>;

    while(<FH>){
        $lib = $_;
        $lib =~ s/^\s+([^\s]+)\s.*$/$1/s;
        if( $lib !~ /^\/System\/Library/ && $lib !~ /^\/usr\/lib/ && $lib !~ /^\@executable_path\// ) {
            $libname = $lib;
            $libname =~ s#^.*/##;
            $targetpath = "$TARGETDIR/$libname";
            $libid = "\@executable_path/../Frameworks/$libname";
            if( !$done{$libname} ) {
                $done{libname} = 1;
                push @worklist, $targetpath;
                system( ("cp", $lib, $targetpath) ) == 0 || die "Failed to copy $lib to $targetpath";
                system( ($NTOOL, "-id", $libid, $targetpath) ) == 0 || die "Failed to set $lib ID to $libid";
                print "Copied $lib => $targetpath\n";
            }
            system( ($NTOOL, "-change", $lib, $libid, $target ) ) == 0 || die "Failed to change $lib ID to $libid";
        }
    }
    close FH;
}
