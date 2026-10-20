import json
import os.path
import shutil

top = '.'
out = 'build'


def options(ctx):
    ctx.load('pebble_sdk')


def configure(ctx):
    ctx.load('pebble_sdk')


def app_version(ctx):
    """Version from package.json."""
    with open(ctx.path.find_node('package.json').abspath()) as f:
        return json.load(f)['version']


def ensure_lootlocker_config(ctx):
    """lootlocker_config.js is git-ignored; a fresh clone builds from the example (leaderboard off)."""
    pkjs = ctx.path.find_node('src/pkjs').abspath()
    config = os.path.join(pkjs, 'lootlocker_config.js')
    if not os.path.exists(config):
        shutil.copyfile(os.path.join(pkjs, 'lootlocker_config.example.js'), config)
        print('Created src/pkjs/lootlocker_config.js from the example; see docs/LEADERBOARD_SETUP.md')


def build(ctx):
    ctx.load('pebble_sdk')
    version = app_version(ctx)
    ensure_lootlocker_config(ctx)

    binaries = []
    cached_env = ctx.env
    for platform in ctx.env.TARGET_PLATFORMS:
        ctx.env = ctx.all_envs[platform]
        ctx.env.append_value('DEFINES', ['APP_VERSION="{}"'.format(version)])
        ctx.set_group(ctx.env.PLATFORM_NAME)
        app_elf = '{}/pebble-app.elf'.format(ctx.env.BUILD_DIR)
        ctx.pbl_build(source=ctx.path.ant_glob('src/c/**/*.c'), target=app_elf, bin_type='app')
        binaries.append({'platform': platform, 'app_elf': app_elf})
    ctx.env = cached_env

    ctx.set_group('bundle')
    ctx.pbl_bundle(binaries=binaries,
                   js=ctx.path.ant_glob(['src/pkjs/**/*.js', 'src/pkjs/**/*.json']),
                   js_entry_file='src/pkjs/index.js')

    def copy_versioned_pbw(ctx):
        """Next to the SDK's <folder>.pbw, keep build/refinery-defense-<version>.pbw."""
        out = ctx.path.get_bld().abspath()
        pbws = [f for f in os.listdir(out) if f.endswith('.pbw') and not f.startswith('refinery-defense-')]
        for name in pbws:
            dst = os.path.join(out, 'refinery-defense-{}.pbw'.format(version))
            shutil.copyfile(os.path.join(out, name), dst)
            print('Versioned bundle: build/{}'.format(os.path.basename(dst)))

    ctx.add_post_fun(copy_versioned_pbw)
