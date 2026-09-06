use strict;
use warnings FATAL => "all";

use Cwd;
use File::Find;
use File::Basename;
use File::Path qw/make_path mkpath rmtree/;
use File::Copy;
use File::Spec::Functions;
use List::Util qw/max sum/;
use List::MoreUtils qw/uniq/;
use Term::ANSIColor;

##################    configuration section    ##################

my $SWOS_16_17  = 0;    # build for SWOS 16/17 edition
my $SENSI_DAYS  = 0;    # version without the multiplayer, meant for tournaments

# SWOS++ base file name
my $BASE_FNAME  = 'swospp';

# list of ignored files, file name without extension will cause all files with
# that name to be ignored regardless of their extension
my %IGNORE_SRCS = map { $_ => 1 } qw//;
my %IGNORE_DIRS = map { $_ => 1 } qw//;

my $CC          = 'g++';
my $AS          = '..\extra\nasm';
my $DIS         = '';       # no need for wdis anymore, gcc can generate listings directly
my $LINK        = 'alink';
my $PCH         = 'stdinc.h';

my $CFLAGS      = '-m32 -c -Wall -Wextra -std=c++11 -mregparm=3 -masm=intel -O3 -Wa,-adhlns=$(LST_DIR)/$*.lst ' .
                  '-fno-ident -Winvalid-pch -mno-red-zone -mrtd -march=i386 -nostdlib -ffreestanding ' .
                  '-fno-leading-underscore -fno-stack-protector -mpush-args -mno-accumulate-outgoing-args ' .
                  '-mno-stack-arg-probe -fno-exceptions -fno-unwind-tables -fno-asynchronous-unwind-tables ' .
                  '-momit-leaf-frame-pointer -mpreferred-stack-boundary=2 -Wundef -fomit-frame-pointer ' .
                  '-fverbose-asm -Wstack-usage=43008 -Wno-multichar -Wno-class-memaccess ' .
                  '-Wno-stringop-overread -Wno-stringop-overflow -o$@ $<';
my $AFLAGS      = '-s -w+macro-params -w+orphan-labels -fwin32 -O3 -o$@ -l$(LST_DIR)/$*.asm.lst $<';

my $CPP_INCLUDE_SWITCH = '-I';
my $ASM_INCLUDE_SWITCH = '-I';

my $DBG_CFLAGS  = ' -DDEBUG=1 -g';
my $DBG_AFLAGS  = ' -DDEBUG';

# keep everything in one code and one data section, bss goes into data
my $LFLAGS =  <<'EOF';
    /merge:_BSS=.data /merge:CONST2=.data /merge:CONST=.data
    /merge:.strdata=.data /merge:_DATA=.data /merge:_TEXT=.text
    /merge:.text.unlikely=.text /merge:.rdata=.data
    /section-flags:.data=0xe07000c0 /section-align:4096 /entry:start
    /m+ /export:PatchSize /export:PatchStart /output-format:PE
    /verbosity:1 /file-align:8
EOF

my $OBJ_DIR = '../obj';
my $BIN_DIR = '../bin';
my $VAR_DIR = '../var';
my $LST_DIR = '../var';
my $BMP_DIR = '../bitmap';

# directories to copy final binary to
my $dirList = $ENV{'SWOS_DIRS'} || 'd:\\games\\swos2;d:\\games\\swos3';
$dirList =~ s/^"+|"+$//g;
my @DEST_DIRS = split(/;/, $dirList);


################## end of user modifiable part ##################
use Data::Dumper;

sub parseCommandLine;
sub ensureDirectories;
sub logl;
sub updateTimestamp;
sub generateBitmaps;
sub buildFiles;
sub runCommand;
sub filterCommandOutput;
sub createLnkFile;
sub linkFiles;
sub createBin;
sub recreateCommandLine;
sub showWarningReport;
sub handleSWOSAnniversaryVersion;
sub handleOfflineVersion;

my $FILENAME = $BASE_FNAME . '.bin';
my $LNK_FILE = $BASE_FNAME . '.lnk';
my $EXE_FILE = $BASE_FNAME . '.exe';

# declare everything here
my %PARAMS;
my $TARGET;
my $CLEAN = 0;
my $DEBUG = 0;
my $REBUILD = 0;
my %additionalIncludes;
my %srcFiles;
my %includes;
my %warnings;
my $PCH_OUTPUT = $OBJ_DIR;

# parse command line and set some flags
parseCommandLine();
if ($CLEAN) {
    logl("Cleaning...");
    clean();
    exit(0) if ($PARAMS{'target'} eq 'clean');
    $PARAMS{'target'} = 'all';  # assuming it was 'rebuild' at this point
}

# set debug flags
if ($TARGET eq 'dbg') {
    $CFLAGS .= $DBG_CFLAGS;
    $CFLAGS =~ s/-O3/-Og/g;
    $AFLAGS .= $DBG_AFLAGS;
}
ensureDirectories();

# make script will be secret implicit dependency of each file
my $scriptTimestamp = getTimestamp($0);
# if we have to rebuild make it as if script changed
$scriptTimestamp = time() if ($REBUILD);

generateBitmaps($scriptTimestamp);

my $TARGET_FULL = ('RELEASE', 'DEBUG')[$TARGET eq 'dbg'];
$OBJ_DIR = catdir($OBJ_DIR, $TARGET);
$PCH_OUTPUT = catfile($OBJ_DIR, "$PCH.gch");
my %SRC_EXTENSIONS = map { $_ => 1 } qw/.c .cpp .asm/;
my %INCLUDES_EXTENSIONS = map { $_ => 1 } qw/inc h/;

handleSWOSAnniversaryVersion();
handleOfflineVersion();

# go into file loop
logl("Traversing source directories...");
find({ wanted => sub
{
    my $path = canonpath(catdir('.', $File::Find::name));
    my ($base, $dir, $ext) = fileparse($path, qr/\.[^.]*/);
    my $file = $base . $ext;

    # skip files from debug directory if we're not building debug version
    return if (substr($path, 0, 5) eq 'debug' && $TARGET ne 'dbg');
    return if ($IGNORE_SRCS{$file} || $IGNORE_SRCS{$base});
    return if ($IGNORE_DIRS{$dir});

    # each subdirectory will be additional include path
    if (-d) {
        $base ne 'deps' or die "Special dir name 'deps' is used for storing dependencies.\n";
        $additionalIncludes{$path} = 1;
        if (File::Spec->rel2abs(getcwd()) ne File::Spec->rel2abs($path)) {
            my $dir = catdir($OBJ_DIR, $path);
            -d $dir or make_path($dir) or die "Couldn't create directory '$dir'.\n";
            return;
        }
    }

    return if !-f;

    if ($SRC_EXTENSIONS{$ext}) {
        logl("Found source file '$path'.");
        $srcFiles{$path} = newSrcFile($path, $dir, $file, $base, $ext);
    } elsif (length($ext) > 1 && $INCLUDES_EXTENSIONS{substr($ext, 1)}) {
        logl("Found include file '$path'.");
        !$includes{$file} or die "Duplicate include file found! ($path)\n";
        $includes{$file} = newIncludeFile($path);
    }
}, no_chdir => 1 }, '.');
logl(Dumper(%includes));

my $pchBuilt = buildPrecompiledHeader($scriptTimestamp);

# gather dependencies
logl("Resolving dependencies and updating timestamps...");
map {
    # skip if precompiled headers were generated
    my $srcPath = $_;
    my $isCppFile = $srcFiles{$srcPath}{'ext'} eq '.c' || $srcFiles{$srcPath}{'ext'} eq '.cpp';

    if ($pchBuilt && $isCppFile) {
        $srcFiles{$srcPath}{'build'} = 1;
    } else {
        getDependencies($srcPath, $srcFiles{$srcPath}{'dir'}, $srcFiles{$srcPath}{'file'});
        updateTimestamp($srcPath);
        # we have dependencies for this file, as well as joint timestamp, so push it to the list if it needs to be built
        # also force rebuild if make script changed
        $srcFiles{$srcPath}{'build'} = $srcFiles{$srcPath}{'timestamp'} > $srcFiles{$srcPath}{'objTimestamp'} ||
            $scriptTimestamp > $srcFiles{$srcPath}{'objTimestamp'};
    }
} keys %srcFiles;

# generate build list after all the files have been processed since some might turn out
# to be includes with wrong extension; only then can this be determined with certainty
my @buildList = grep { $srcFiles{$_}{'build'} } keys %srcFiles;

logl("Source files:");
logl(Dumper(\%srcFiles));
logl("Includes:");
logl(Dumper(\%includes));
logl("Additional include directories:");
logl(Dumper(\%additionalIncludes));
logl("Files to be built:");
logl(Dumper(\@buildList));

buildFiles(\@buildList);
createLnkFile($scriptTimestamp);
linkFiles($scriptTimestamp);
createBin($scriptTimestamp);
showWarningReport();

# todo: check for dbg to rel builds, and vice versa and copy corresponding bin

if ($PARAMS{'target'} eq 'all') {
    $PARAMS{'target'} = 'rel';  # all should've done the job of dbg
    logl('Recursive call: ', 'perl ', $0, ' ', recreateCommandLine());
    exec('perl', $0 . ' ' . recreateCommandLine()) or die "Recursive call failed.\n";
}
# todo: iterate through dep files and delete those that have no corresponding source file


sub formDepFileName
{
    my ($dir, $file) = @_;
    $file = join('.', File::Spec->splitdir($dir)) . $file;
    $file = substr($file, 2) if (substr($file, 0, 2) eq '..');

    return catdir(catdir($OBJ_DIR, 'deps'), $file) . '.dep';
}


sub markAsScanned
{
    my ($path, $file, $isInclude, $dependencies) = @_;
    my $hashRef = $isInclude ? $includes{$file} : $srcFiles{$path};
    $hashRef->{'depsScanned'} = 1;

    return $hashRef->{'dependencies'} = $dependencies;
}


# execute a search for include file in all directories, and return path to it
sub findInclude
{
    my ($file, $parent) = @_;
    return $file if (-e $file);

    foreach my $dir (keys %additionalIncludes) {
        my $path = catdir($dir, $file);
        return $path if (-e $path);
    }

    die "Included file $file (from $parent) could not be found.\n";
}


# scan include file for it's own dependencies, and add them to array given by reference
sub scanIncludeDependencies
{
    my ($file, $dependencies, $parent) = @_;
    my $incPath = findInclude($file, $parent);

    # reassign source file to includes if it's being included
    if (exists $srcFiles{$incPath}) {
        logl("Moving source file $incPath to includes.");
        $includes{$file} = $srcFiles{$incPath};
        delete $srcFiles{$incPath};
    }

    my ($incBase, $incDir, $incExt) = fileparse($incPath, qr/\.[^.]*/);
    my $incFile = $incBase . $incExt;
    push(@{$dependencies}, $incFile);

    # recursively check for sub-dependencies and appropriate them ;)
    my $subDependencies = getDependencies($incPath, $incDir, $incFile, $parent);
    push(@{$dependencies}, $_) for @{$subDependencies};
}


sub getIncludeRegex
{
    my ($ext) = @_;
    # '#include' for C/C++, '%include' and 'incbin' for NASM
    use constant C_INCLUDE_REGEX => '#\s*(include)';
    use constant ASM_INCLUDE_REGEX => '(incbin|%\s*include)';
    my %directives = (
        'c' => \C_INCLUDE_REGEX, 'cpp' => \C_INCLUDE_REGEX, 'h' => \C_INCLUDE_REGEX,
        'asm' => \ASM_INCLUDE_REGEX, 'inc' => \ASM_INCLUDE_REGEX
    );
    my $directives = $directives{$ext} or die "Unknown extension encountered: \".$ext\".\n";

    return qr/^\s*${$directives}\s*"([a-zA-Z0-9_.\/]+)"/;
}


# return reference to array of dependencies for a given file
sub getDependencies
{
    my ($path, $dir, $file, $parent) = @_;
    my $dependencies = [];
    my ($IN, $OUT, $DEP);

    # special case when a source file has been transfered to includes
    return [] if (!$parent && !exists $srcFiles{$path});

    # check memory first
    if (!$parent && $srcFiles{$path}{'depsScanned'}) {
        return $srcFiles{$path}{'dependencies'};
    } elsif ($parent && exists($includes{$file}) && $includes{$file}{'depsScanned'}) {
        return $includes{$file}{'dependencies'};
    }

    # form dependency file name taking care of directories
    my $depFile = formDepFileName($dir, $file);
    my $timestamp;

    # this is more of internal check.. see if the files that are supposed to be there are really there
    if ($parent) {
        if (!exists($includes{$path})) {
            -e $path or die("Include file $file ($path), included from $parent, could not be found.\n");
            $includes{$file} = newIncludeFile($path);
        }
        $timestamp = $includes{$file}{'timestamp'};
    } else {
        exists($srcFiles{$path}{'timestamp'}) or die("Error, source file $path is missing.\n");
        $timestamp = $srcFiles{$path}{'timestamp'};
    }

    # if .dep file is newer than us just read from it
    my $isBinary = -B $path;
    if (!$isBinary && getTimestamp($depFile) > $timestamp) {
        my @depContents;
        open($DEP, '<', "$depFile") or die "Failed to open file: $depFile\n";
        chomp(@depContents = <$DEP>);
        close($DEP);
        # don't forget about sub-dependencies!
        map { scanIncludeDependencies($_, $dependencies, $file) } @depContents;
        $dependencies = [uniq(@{$dependencies})];
        markAsScanned($path, $file, defined($parent) && length($parent) > 0, $dependencies);
        return $dependencies;
    }

    # do a crude scan for includes; it doesn't account for say conditionally included files,
    # make sure to expand it if it's ever needed; we're also assuming user will be nice and
    # wouldn't use <> for non-system includes
    # but don't attempt to scan binary files included via incbin
    if (!$isBinary) {
        open($IN, '<', "$path") or die "Failed to open file: $path\n";

        $path =~ /\.([^.]+)$/;

        my $incRegex = getIncludeRegex($1);
        while (<$IN>) {
            if (/$incRegex/) {
                # dependencies of our includes are also our dependencies
                scanIncludeDependencies($2, $dependencies, $file);
            }
        }

        close($IN);
    }

    # remove duplicates
    $dependencies = [uniq(@{$dependencies})];

    # finally create and fill dependencies file
    if (!$isBinary && @{$dependencies}) {
        open($OUT, '>', "$depFile") or die "Failed to create dependency file $depFile\n";
        print $OUT "$_\n" for @{$dependencies};
        close($OUT);
    }

    # we're done, just mark it as scanned
    return markAsScanned($path, $file, defined($parent) && length($parent) > 0, $dependencies);
}


sub newSrcFile
{
    my ($path, $dir, $file, $base, $ext) = @_;
    my %srcFile;
    $srcFile{'file'} = $file;
    $srcFile{'dir'} = $dir;
    $srcFile{'path'} = $path;
    $srcFile{'base'} = $base;
    $srcFile{'ext'} = $ext;
    $srcFile{'timestamp'} = (stat $path)[9];
    $srcFile{'depsScanned'} = 0;
    # append extension so we can handle asm and C++ files with same names
    $srcFile{'obj'} = catdir($OBJ_DIR, $dir, "$base$ext.obj");
    unlink($srcFile{'obj'}) if (-z $srcFile{'obj'});
    rmtree($srcFile{'obj'}) if (-d $srcFile{'obj'});
    $srcFile{'objTimestamp'} = getTimestamp($srcFile{'obj'});
    $srcFile{'build'} = 0;
    return \%srcFile;
}


sub newIncludeFile
{
    my $path = $_[0];
    my %incFile;
    $incFile{'timestamp'} = (stat $path)[9];
    $incFile{'depsScanned'} = 0;
    $incFile{'path'} = $path;
    return \%incFile;
}


sub assignTarget
{
    my ($target, $realTarget) = @_;
    $TARGET and die "More than one target specified.\n";
    $TARGET = $target;
    $PARAMS{'target'} = $realTarget;
}


sub getScriptInfo
{
    return 'SWOS++ swos-port helper build script v1.0 by Zlatko Karakas';
}


sub printVersionInfo
{
    print getScriptInfo(), "\n";
}


sub printHelp
{
    print getScriptInfo(),
        "\nTargets:\n",
        "clean   - remove all files from previous build\n",
        "rebuild_dbg - rebuild debug version\n",
        "rebuild_rel - rebuild release version\n",
        "all     - build both debug and release version\n",
        "dbg     - build debug version\n",
        "rel     - build release version\n",
        "dist    - build version for distribution\n",
        "Switches:\n",
        "-d      - do not build, just print commands and debug info\n",
        "-p      - print commands before execution\n",
        "-e      - redirect stderr to stdout\n",
        "-v      - show version and quit\n",
        "-h      - show help and exit";
}


sub parseCommandLine
{
    my ($arg) = @_;
    my %switches = (
        'clean' => sub { assignTarget('clean', 'clean'); $CLEAN = 1; },
        'all'   => sub { assignTarget('dbg', 'all'); },
        'dbg'   => sub { assignTarget('dbg', 'dbg'); },
        'rel'   => sub { assignTarget('rel', 'rel'); },
        'dist'  => sub { die("Target 'dist' not supported yet.\n"); },
        '-d'    => sub { $DEBUG = 1; },
        '-p'    => sub { $PARAMS{'verbose'} = 1; },
        '-e'    => sub { open(STDERR, ">&STDOUT"); },
        'rebuild_dbg'   => sub { assignTarget('dbg', 'rebuild_dbg'); $REBUILD = 1; },
        'rebuild_rel'   => sub { assignTarget('rel', 'rebuild_rel'); $REBUILD = 1; },
        '-h'    => sub { printHelp(); exit(0); },
        '-v'    => sub { printVersionInfo(); exit(0); },
        '--help'    => sub { printHelp(); exit(0); },
        '--version' => sub { printVersionInfo(); exit(0); },
    );

    foreach my $arg (@ARGV) {
        if ($switches{$arg}) {
            $switches{$arg}->();
        } else {
            die "Unrecognized command: $arg\n";
        }
    }

    # build debug by default
    $PARAMS{'target'} ||= 'dbg';
    $TARGET ||= 'dbg';
    # not verbose by default
    $PARAMS{'verbose'} ||= 0;
}


# make sure all the directories we require are present
sub ensureDirectories
{
    make_path(catdir(catdir($OBJ_DIR, $TARGET), 'deps'));
}


sub clean
{
    if ($DEBUG) {
        print join(' ', glob(catdir($BMP_DIR, '*.bp'))), "\n";
        print "rmtree($OBJ_DIR, { keep_root => 1});\n";
        print "rmtree($VAR_DIR, { keep_root => 1});\n";
    } else {
        unlink glob(catdir($BMP_DIR, '*.bp'));
        rmtree($OBJ_DIR, { keep_root => 1});
        rmtree($VAR_DIR, { keep_root => 1});
    }
}


# set timestamp of every source file to maximum of it's own + it's dependencies
sub updateTimestamp
{
    my ($path) = @_;
    $srcFiles{$path}{'timestamp'} =
        max($srcFiles{$path}{'timestamp'}, map { $includes{$_}{'timestamp'} } @{$srcFiles{$path}{'dependencies'}});
}


sub generateBitmaps
{
    logl("Generating bitmaps...");
    my ($scriptTimestamp) = @_;

    use constant BMP_GEN_SCRIPT => 'bmpcvt.pl';
    # go with whichever is newer
    my $bmpScriptTimestamp = max(getTimestamp(BMP_GEN_SCRIPT), $scriptTimestamp);

    -e catdir($BMP_DIR, BMP_GEN_SCRIPT) or die "Bitmap generation script ", BMP_GEN_SCRIPT, " is missing.\n";

    opendir(my $DIR, $BMP_DIR);
    my @files = map { s/\.[^.]+$//; $_ } grep {/\.bmp$/ } readdir $DIR;
    my %parmMap = (
        'swospp-logo.bmp' => [ '0', '-g', ],
        'swos_united_logo.bmp' => [ '15', ],
    );

    foreach my $file (@files) {
        my $bmpFile = "$file.bmp";
        my $parms = exists($parmMap{$bmpFile}) ? $parmMap{$bmpFile} : [];
        my $bmpTimestamp = getTimestamp(catdir($BMP_DIR, $bmpFile));
        my $bpTimestamp = getTimestamp((catdir($BMP_DIR, $file . '.bp')));
        if ($bmpTimestamp >= $bpTimestamp || $bmpScriptTimestamp > $bpTimestamp) {
            print "Extracting $bmpFile...\n";
            runCommand('perl', catdir($BMP_DIR, BMP_GEN_SCRIPT), catdir($BMP_DIR, $bmpFile), @$parms);
        }
    }

    closedir($DIR);
    $additionalIncludes{'..\bitmap'} = 1;   # for possible includes via NASM's incbin
}


sub runCommand
{
    if ($PARAMS{'verbose'} || $DEBUG) {
        print 'COMMAND: ' . join(' ', @_) . "\n";
        return if $DEBUG;
    }

    system(@_) == 0 or die "Failed to run command: " . join(' ', @_) . ", ($?)\n";
    if ($? >> 8) {
        showWarningReport();
        exit($? >> 8);
    }
}


sub filterCommandOutput
{
    my ($cmd, $ignorePatternsOrCaptureFile, $file) = @_;
    my @output;
    if ($PARAMS{'verbose'} || $DEBUG) {
        print "COMMAND: $cmd, IGNORE: @{$ignorePatternsOrCaptureFile} \n";
        return if $DEBUG;
    }

    @output = `$cmd`;
    $? == -1 and die "Command failed: $cmd\n";
    if ((ref($ignorePatternsOrCaptureFile) eq 'ARRAY') && scalar(@{$ignorePatternsOrCaptureFile})) {
        OUTPUT: foreach my $line (@output) {
            foreach (@{$ignorePatternsOrCaptureFile}) {
                next OUTPUT if (index($line, $_) != -1);
            }

            $warnings{$file} = $1 if ($line =~ /(\d+) warnings/ && $1 > 0);
            print $line;
        }
    } else {
        open(my $CAPTURE, '>', $ignorePatternsOrCaptureFile) or
            die "Failed to create capture file $ignorePatternsOrCaptureFile: $!.\n";
        print $CAPTURE @output;
        close $CAPTURE;
    }
    if ($? >> 8) {
        showWarningReport();
        exit($? >> 8);
    }
}


sub logl
{
    print @_, "\n" if ($DEBUG);
}


sub handleSWOSAnniversaryVersion
{
    if ($SWOS_16_17) {
        $CFLAGS .= ' -DSWOS_16_17=1';
        $AFLAGS .= ' -DSWOS_16_17';
    } else {
        $IGNORE_SRCS{'swos1617'} = 1;
    }
}


sub handleOfflineVersion
{
    if ($SENSI_DAYS) {
        if ($SWOS_16_17) {
            print "Warning: offline version overriding anniversary version.\n";
            $SWOS_16_17 = 0;
            handleSWOSAnniversaryVersion();
        }

        $IGNORE_DIRS{'mplayer\\'} = 1;

        $CFLAGS .= ' -DSENSI_DAYS=1';
        $AFLAGS .= ' -DSENSI_DAYS';
    }
}


sub getAdditionalIncludes
{
    my ($switch) = @_;
    return join(' ', map { "$switch$_" } keys %additionalIncludes);
}


sub expandCommandVariables
{
    my ($cmd, $path, $obj, $base, $lstDir) = @_;
    return replace($cmd, {
        '$<' => $path,
        '$@' => $obj,
        '$*' => $base,
        '$(LST_DIR)' => $lstDir,
    });
}


# expects to run before dependencies resolution
sub buildPrecompiledHeader
{
    my ($scriptTimestamp) = @_;
    my ($base, $dirs, $ext) = fileparse($PCH, qr/\.[^.]*/);

    $srcFiles{$PCH} = newSrcFile($PCH, $dirs, $PCH, $base, $ext);
    getDependencies($PCH, '', $PCH);
    updateTimestamp($PCH);
    my $gchTimestamp = getTimestamp($PCH_OUTPUT);
    my $pchTimestamp = $srcFiles{$PCH}{'timestamp'};
    delete $srcFiles{$PCH};

    if ($pchTimestamp > $gchTimestamp || $scriptTimestamp > $gchTimestamp) {
        print "Generating precompiled headers...\n";
        my $cmd = $CC . ' ' . getAdditionalIncludes($CPP_INCLUDE_SWITCH) . ' -x c++-header ' . $CFLAGS;
        runCommand(expandCommandVariables($cmd, $PCH, $PCH_OUTPUT, $PCH, $LST_DIR));
        return 1;
    }

    return 0;
}


sub buildFiles
{
    my ($buildList) = @_;
    my $cIncludeDirs = getAdditionalIncludes($CPP_INCLUDE_SWITCH);
    my $asmIncludeDirs = getAdditionalIncludes($ASM_INCLUDE_SWITCH);
    my $C_CMD_LINE = $CC . ' -include ' . catfile($OBJ_DIR, $PCH) . ' ' . $cIncludeDirs . ' ' . $CFLAGS;
    my $ASM_CMD_LINE = $AS . ' ' . $asmIncludeDirs . ' ' . $AFLAGS;
    my %buildCommands = ('.c' => $C_CMD_LINE, '.cpp' => $C_CMD_LINE, '.asm' => $ASM_CMD_LINE);
    my $targetTag = "[$TARGET_FULL] ";

    # sort build list by path, and asm files before cpp
    @buildList = sort {
        if ($srcFiles{$a}{'ext'} eq $srcFiles{$b}{'ext'}) {
            # put root files in front
            my $isRootA = index($srcFiles{$a}{'path'}, '\\') == -1;
            my $isRootB = index($srcFiles{$b}{'path'}, '\\') == -1;
            if ($isRootA && !$isRootB) {
                return -1;
            } elsif (!$isRootA && $isRootB) {
                return 1;
            }
            return $srcFiles{$a}{'path'} cmp $srcFiles{$b}{'path'};
        } else {
            return $srcFiles{$a}{'ext'} eq '.asm' ? -1 : 1
        }
    } @buildList;

    foreach my $src (@{$buildList}) {
        exists $buildCommands{$srcFiles{$src}{'ext'}}
            or die "Invalid extension for source files '$srcFiles{$src}{'ext'}' found.\n";

        mkpath($srcFiles{$src}{'dir'});
        my $ext = $srcFiles{$src}{'ext'};
        print $targetTag, ('Assembling ', 'Compiling ')[$ext eq '.c' || $ext eq '.cpp'],
            $srcFiles{$src}{'file'}, "...\n";

        # expand commands
        my $cmd = $buildCommands{$srcFiles{$src}{'ext'}};
        runCommand(expandCommandVariables($cmd, $srcFiles{$src}{'path'},
            $srcFiles{$src}{'obj'}, $srcFiles{$src}{'base'}, $LST_DIR));

        if ($DIS ne '' && ($ext eq '.c' || $ext eq '.cpp')) {
            # disassemble; note that this will make listings of files with same names
            # in different subdirectories overwrite each other
            filterCommandOutput($DIS . ' ' . $srcFiles{$src}{'obj'} . " /s=$srcFiles{$src}{'path'}",
                catdir($LST_DIR, $srcFiles{$src}{'base'}) . $ext . '.lst');
        }

        # if we get here, everything went well with compiling/assembling, so update our internal timestamps
        $srcFiles{$src}{'objTimestamp'} = time();
    }
}


sub createLnkFile
{
    my ($scriptTimestamp) = @_;
    my $lnkFile = catdir($OBJ_DIR, $LNK_FILE);
    my $lnkTimestamp = getTimestamp($lnkFile);
    my $latestDepTimestamp = max(map({ $srcFiles{$_}{'objTimestamp'} } keys %srcFiles), $scriptTimestamp);
    logl("createLnkFile: script ts = $scriptTimestamp, dep ts = $latestDepTimestamp, lnk ts = $lnkTimestamp");

    if ($latestDepTimestamp > $lnkTimestamp) {
        $LFLAGS =~ tr/\r\n//d;
        my $lnkContents = $LFLAGS . ' ' . join(' ', map({ $srcFiles{$_}{'obj'} } keys %srcFiles)) .
            ' /output-name:' . catdir($OBJ_DIR, $EXE_FILE);
        logl("LNK FILE CONTENTS:\n$lnkContents");
        return 1 if ($DEBUG);
        open my $LNK, '>', $lnkFile or die "Failed to create $lnkFile\n";
        print $LNK $lnkContents;
        close $LNK;
    }
}


sub linkFiles
{
    my ($scriptTimestamp) = @_;
    my $lnkFile = catdir($OBJ_DIR, $LNK_FILE);
    my $exeFileTimestamp = getTimestamp(catdir($OBJ_DIR, $EXE_FILE));

    if ($exeFileTimestamp < getTimestamp($lnkFile) || $exeFileTimestamp < $scriptTimestamp) {
        print "Linking...\n";
        runCommand($LINK, '@' . $lnkFile);
        my $srcMap = catdir($OBJ_DIR, "$BASE_FNAME.map");
        my $dstMap = catdir($VAR_DIR, "${BASE_FNAME}_$TARGET.map");
        if ($DEBUG) {
            print "MOVE: $srcMap -> $dstMap\n";
        } else {
            move($srcMap, $dstMap) or die "Error moving map file.\n";
        }
        print "PE Executable linked: $BASE_FNAME.exe [$TARGET_FULL VERSION]\n";
    }
}


sub createBin
{
    my ($scriptTimestamp) = @_;
    my $exeFile = catdir($OBJ_DIR, $EXE_FILE);
    my $binFile = catdir($OBJ_DIR, $FILENAME);
    my $binFileTimestamp = getTimestamp($binFile);
    my $pe2bin = catdir($BIN_DIR, 'pe2bin.exe');
    my $pe2binTimestamp = getTimestamp($pe2bin);

    if ($binFileTimestamp < getTimestamp($exeFile) || $binFileTimestamp < $scriptTimestamp || $binFileTimestamp < $pe2binTimestamp) {
        if (!$DEBUG) {
            open(my $F, '>', catdir($BIN_DIR, "$TARGET_FULL")) or die "Failed to create $TARGET marker file.\n";
            print $F "$TARGET_FULL VERSION\n";
            close $F;
            unlink(catdir($BIN_DIR, ('DEBUG', 'RELEASE')[$TARGET eq 'dbg']));
        }

        runCommand($pe2bin, $exeFile);
        my $result = catdir($BIN_DIR, $FILENAME);
        logl("COPY: $binFile -> $result");
        $DEBUG or copy($binFile, $result) or die "Failed to copy $FILENAME from $OBJ_DIR to $BIN_DIR.\n";
        foreach my $destDir (@DEST_DIRS) {
            logl("COPY: $binFile -> $destDir");
            $DEBUG or copy($binFile, $destDir) or die("Failed to copy $FILENAME from $OBJ_DIR to $destDir. ($!)\n");
        }
    }
}


# return timestamp of a file, or 0 if not existing
sub getTimestamp
{
    my ($file) = @_;
    my $ts = (stat $file)[9];
    $ts ||= 0;
    return $ts;
}


sub replace
{
    my ($str, $replaceList) = @_;
    while (my ($findVal, $replaceVal) = each %{$replaceList}) {
        my $pos = index($str, $findVal);
        while ($pos > -1) {
            substr($str, $pos, length($findVal), $replaceVal);
            $pos = index($str, $findVal, $pos + length($replaceVal));
        }
    }

    return $str;
}


sub recreateCommandLine
{
    my $cmdLine;
    $cmdLine .= $PARAMS{'target'};
    $cmdLine .= ' -d' if $DEBUG;
    $cmdLine .= ' -p' if $PARAMS{'verbose'};
    return $cmdLine;
}


sub showWarningReport
{
    # warnings get lost amongst tons of output scrolling lightning fast
    if (%warnings) {
        my $totalWarnings = sum(values %warnings);
        my $numFilesPrinted = 0;
        my $warningReport = '';

        foreach my $file (keys %warnings) {
            last if $numFilesPrinted >= 5;    # max 5 files
            $warningReport .= "$file($warnings{$file}) ";
            $numFilesPrinted++;
        }

        substr($warningReport, -1, 1, '');
        $warningReport .= '...' if ($numFilesPrinted < scalar keys %warnings);
        print color 'red';
        print "$totalWarnings warning(s):\n$warningReport\n";
    }
}
