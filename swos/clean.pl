#!/usr/bin/perl

# Clean up the IDA generated ASM file from non-ASCII chars in the same way ida2asm does

if (@ARGV < 1) {
    die "Missing input filename argument.\n";
} elsif (@ARGV < 2) {
    die "Missing output filename argument.\n";
}

open my $inFile, "<:raw", $ARGV[0] or die "Could not open file: $ARGV[0] for reading.\n";
binmode $inFile;
open my $outFile, ">:raw", $ARGV[1] or die "Could not open file: $ARGV[1] for writing.\n";
binmode $outFile;

while (read($inFile, my $byte, 1)) {
    $byte = unpack('C', $byte);

    if ($byte == ord("\x18") || $byte == ord("\x19")) {
        $newByte = ord('|');
    } elsif ($byte == ord("\xcd") || $byte == ord("\xcb")) {
        $newByte = ord('=');
    } elsif ($byte == ord("\x10")) {
        $newByte = ord('>');
    } elsif ($byte > 127) {
        $newByte = ord('_');
    } else {
        $newByte = $byte;
    }

    print $outFile pack('C', $newByte);
}
