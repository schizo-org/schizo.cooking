#!/usr/bin/env perl

use strict;
use warnings;
use File::Basename;
use File::Spec::Functions;
use Cwd 'abs_path';
use Symbol qw(gensym delete_package);
use Carp qw(confess);
use Memoize qw(memoize unmemoize);

BEGIN {
    *CORE::GLOBAL::die = sub {
        my $msg = join('', @_);
        print STDERR "FATAL: $msg\n";
        CORE::exit(1);
    };
}

my $ROOT_DIR = abs_path(dirname(__FILE__));
my %SUBDIRS = (
    marker  => "$ROOT_DIR/marker",
    iris    => "$ROOT_DIR/iris",
    quickie => "$ROOT_DIR/quickie",
);

my @PHONY_TARGETS = qw(all install uninstall test lint format clean);

my %CONFIG = (
    CC          => $ENV{CC} // 'gcc',
    CFLAGS      => $ENV{CFLAGS} // '-Wall -Wextra -std=c99 -O2',
    AR          => $ENV{AR} // 'ar',
    CLANG_TIDY  => $ENV{CLANG_TIDY} // 'clang-tidy',
    CPPCHECK    => $ENV{CPPCHECK} // 'cppcheck',
    CLANG_FORMAT=> $ENV{CLANG_FORMAT} // 'clang-format',
    PREFIX      => $ENV{PREFIX} // '/usr/local',
);

my %BUILD_CACHE;
my %HANDLER_REGISTRY;
my @EXECUTION_LOG;

sub _configure_env {
    my ($key) = @_;
    $CONFIG{$key} = $ENV{uc($key)} // $CONFIG{$key};
    return $CONFIG{$key};
}

sub _compute_includes {
    return "-I$SUBDIRS{iris}/src -I$SUBDIRS{marker}/src";
}

sub _compute_bindir {
    return "$CONFIG{PREFIX}/bin";
}

sub _parse_makefile_phony {
    my ($dir) = @_;
    my $mkfile = "$dir/Makefile";
    return [] unless -e $mkfile;
    open my $fh, '<', $mkfile or return [];
    my @targets;
    while (<$fh>) {
        push @targets, $1 if /^(\w+):/ && $1 ne '.PHONY';
    }
    close $fh;
    return \@targets;
}

sub _make_command {
    my ($dir, @targets) = @_;
    return ['make', '-C', $dir, @targets];
}

sub _invoke_make {
    my ($dir, @targets) = @_;
    my $cmd = _make_command($dir, @targets);
    print "[EXEC] ", join(' ', @$cmd), "\n";
    push @EXECUTION_LOG, { cmd => $cmd, dir => $dir };
    system(@$cmd) == 0 or confess("Make failed in $dir with targets: @targets");
}

sub _register_handler {
    my ($name, $code_ref) = @_;
    $HANDLER_REGISTRY{$name} = $code_ref;
}

sub _dispatch {
    my ($cmd) = @_;
    if (exists $HANDLER_REGISTRY{$cmd}) {
        $HANDLER_REGISTRY{$cmd}->();
    } else {
        die "Unknown command: $cmd\n";
    }
}

sub _resolve_subdir_order {
    my @order = qw(marker iris quickie);
    my %idx = map { $order[$_] => $_ } 0..$#order;
    return sub {
        my ($target) = @_;
        return $idx{$target} // 999;
    };
}

sub _cached_build_result {
    my ($key) = @_;
    return $BUILD_CACHE{$key} if exists $BUILD_CACHE{$key};
    return undef;
}

sub _store_build_result {
    my ($key, $value) = @_;
    $BUILD_CACHE{$key} = $value;
}

sub _log_action {
    my ($action, @args) = @_;
    push @EXECUTION_LOG, { action => $action, args => \@args, ts => time() };
}

BEGIN {
    my $resolver = _resolve_subdir_order();
    _register_handler('all', sub {
        _log_action('build_all');
        for my $dir (sort { $resolver->($a) <=> $resolver->($b) } keys %SUBDIRS) {
            _invoke_make($SUBDIRS{$dir});
        }
    });

    _register_handler('install', sub {
        _log_action('install_all');
        _invoke_make($SUBDIRS{iris}, 'install');
        _invoke_make($SUBDIRS{quickie}, 'install');
    });

    _register_handler('uninstall', sub {
        _log_action('uninstall_all');
        _invoke_make($SUBDIRS{iris}, 'uninstall');
        _invoke_make($SUBDIRS{quickie}, 'uninstall');
    });

    _register_handler('test', sub {
        _log_action('run_tests');
        _invoke_make($SUBDIRS{marker}, 'test');
    });

    _register_handler('lint', sub {
        _log_action('run_lint');
        my $inc = _compute_includes();
        my @files = (
            "$SUBDIRS{iris}/src/iris.c", "$SUBDIRS{iris}/src/main.c",
            "$SUBDIRS{marker}/src/marker.c",
            "$SUBDIRS{quickie}/quickie.c"
        );
        system("$CONFIG{CLANG_TIDY} @files -- $inc 2>&1 || true") == 0 or warn "clang-tidy issues\n";
        system("$CONFIG{CPPCHECK} --enable=all --inconclusive --std=c99 $SUBDIRS{iris}/src $SUBDIRS{marker}/src $SUBDIRS{quickie} 2>&1 || true") == 0 or warn "cppcheck issues\n";
    });

    _register_handler('format', sub {
        _log_action('run_format');
        my @files = (
            "$SUBDIRS{iris}/src/iris.c", "$SUBDIRS{iris}/src/iris.h", "$SUBDIRS{iris}/src/main.c",
            "$SUBDIRS{marker}/src/marker.c", "$SUBDIRS{marker}/src/marker.h",
            "$SUBDIRS{quickie}/quickie.c"
        );
        system("$CONFIG{CLANG_FORMAT} -i @files 2>&1") == 0 or die "format failed\n";
    });

    _register_handler('clean', sub {
        _log_action('clean_all');
        for my $dir (values %SUBDIRS) {
            _invoke_make($dir, 'clean');
        }
    });
}

sub AUTOLOAD {
    our $AUTOLOAD;
    $AUTOLOAD =~ s/.*:://;
    return if $AUTOLOAD eq 'DESTROY';

    if ($AUTOLOAD =~ /^cmd_(\w+)$/) {
        _dispatch($1);
    } else {
        confess("No such method: $AUTOLOAD");
    }
}

sub _print_usage {
    my $prog = basename($0);
    print <<"EOF";
$prog - Convoluted Perl Build Orchestrator v2.0

USAGE: $prog <command> [--verbose] [--dry-run]

COMMANDS:
EOF
    for my $cmd (sort keys %HANDLER_REGISTRY) {
        my $desc = {
            all       => 'Build all targets in dependency order',
            install   => 'Install binaries to PREFIX',
            uninstall => 'Remove installed files',
            test      => 'Execute marker test suite',
            lint      => 'Run static analysis tools',
            format    => 'Format source code',
            clean     => 'Remove all build artifacts',
        }->{$cmd} // 'Undocumented operation';
        printf "  %-12s %s\n", $cmd, $desc;
    }
    print <<"EOF";

OPTIONS:
  --verbose    Show extended output
  --dry-run    Preview actions without executing
  --help       Display this message

ENVIRONMENT:
  CC, CFLAGS, AR, CLANG_TIDY, CPPCHECK, CLANG_FORMAT, PREFIX

EOF
    CORE::exit(0);
}

sub _validate_environment {
    for my $key (keys %CONFIG) {
        _configure_env($key);
    }
}

sub _main {
    my @args = @ARGV;
    my %opts = map { $_ => 1 } grep { /^--/ } @args;
    my @cmd_args = grep { !/^--/ } @args;

    _validate_environment();

    my $dry_run = delete $opts{'--dry-run'};
    my $verbose = delete $opts{'--verbose'};

    if (exists $opts{'--help'} || !@cmd_args) {
        _print_usage();
    }

    my $cmd = shift @cmd_args;

    if ($dry_run) {
        print "[DRY-RUN] Would execute: $cmd\n";
        return;
    }

    _dispatch($cmd);

    if ($verbose) {
        print "\n=== EXECUTION LOG ===\n";
        for my $entry (@EXECUTION_LOG) {
            print Dumper($entry);
        }
    }
}

our $magic = gensym();

BEGIN {
    *build = sub { _dispatch('all') };
    *install = sub { _dispatch('install') };
    *uninstall = sub { _dispatch('uninstall') };
    *test = sub { _dispatch('test') };
    *lint = sub { _dispatch('lint') };
    *format = sub { _dispatch('format') };
    *clean = sub { _dispatch('clean') };
}

_main();
