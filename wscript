#
# This file is the default set of rules to compile a Pebble application.
#
# Feel free to customize this to your needs.
#
import json
import os.path
import shutil
import subprocess

top = '.'
out = 'build'


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    """
    This method is used to configure your build. ctx.load(`pebble_sdk`) automatically configures
    a build for each valid platform in `targetPlatforms`. Platform-specific configuration: add your
    change after calling ctx.load('pebble_sdk') and make sure to set the correct environment first.
    Universal configuration: add your change prior to calling ctx.load('pebble_sdk').
    """
    ctx.load('pebble_sdk')


def app_version(ctx):
    """Version from package.json and the short git hash; '+' marks uncommitted changes."""
    with open(ctx.path.find_node('package.json').abspath()) as f:
        version = json.load(f)['version']
    root = ctx.path.abspath()
    try:
        rev = subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD'], cwd=root).decode().strip()
        if subprocess.call(['git', 'diff', '--quiet', 'HEAD'], cwd=root) != 0:
            rev += '+'
    except (OSError, subprocess.CalledProcessError):
        rev = 'nogit'
    return version, rev


def build(ctx):
    ctx.load('pebble_sdk')
    version, rev = app_version(ctx)

    build_worker = os.path.exists('worker_src')
    binaries = []

    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.env.append_value('DEFINES', ['APP_VERSION="{}"'.format(version), 'APP_REV="{}"'.format(rev)])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'), target=app_elf, bin_type='app')

        if build_worker:
            worker_elf = '{}/pebble-worker.elf'.format(ctx.env.BUILD_DIR)
            binaries.append({'platform': platform, 'app_elf': app_elf, 'worker_elf': worker_elf})
            ctx.pbl_build(source=ctx.path.ant_glob('worker_src/c/**/*.c'),
                          target=worker_elf,
                          bin_type='worker')
        else:
            binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js',
                                         'src/pkjs/**/*.json',
                                         'src/common/**/*.js']),
                   js_entry_file='src/pkjs/index.js')

    def copy_versioned_pbw(ctx):
        """Next to the SDK's <folder>.pbw, keep build/refinery-defense-<version>-<rev>.pbw."""
        out = ctx.path.get_bld().abspath()
        pbws = [f for f in os.listdir(out) if f.endswith('.pbw') and not f.startswith('refinery-defense-')]
        for name in pbws:
            dst = os.path.join(out, 'refinery-defense-{}-{}.pbw'.format(version, rev))
            shutil.copyfile(os.path.join(out, name), dst)
            print('Versioned bundle: build/{}'.format(os.path.basename(dst)))

    ctx.add_post_fun(copy_versioned_pbw)
