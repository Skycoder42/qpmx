# qpmx
The Advanced Qt package manager - a frontend/replacement for qpm.

[![Travis Build Status](https://travis-ci.org/Skycoder42/qpmx.svg?branch=master)](https://travis-ci.org/Skycoder42/qpmx)
[![Appveyor Build status](https://ci.appveyor.com/api/projects/status/xve94rbg8ewsg59s?svg=true)](https://ci.appveyor.com/project/Skycoder42/qpmx)

## Features
qpmx is designed as a package manager tool without any backend. It is an advanced tool with support for qpm and git package repositories, and provides a bunch of features to make the usage as easy as possible. The main features ares:

- global source caching, to reduce downloads
- precompiles packages locally to speed up builds
	- but also supports build with sources directly
- fully cross-platform
	- can be run on any "host" platform supported by Qt
	- can compile packages for any Qt platform
- easy and simple qmake integration, with just two additional lines a the pro file
	- automatically added by qpmx on package installation
- Supports translations and local includes for (static) library projects
- Resources and `Q_COREAPP_STARTUP_FUNCTION` work as expected, even when used as compiled library
- can search packages
- Methods for developers:
	- Use a local copy instead of an actual package for debug purpose
		- still supports source and compile builds
	- provide methods to publish packages directly via qpmx

### Backends
qpmx is a tool only, and thus needs external "backends" that provide it with packages. This is done via a simple plugin. Currently, two plugins are provided: git and qpm

#### QPM plugin
With the qpm plugin, qpmx can be used to install qpm packages, without loosing all the advantages like caching. It properly resolves dependencies, can search and is version-aware. It requires you to have qpm installed as well. Because qpm packages are not designed to be precompiled, there migth be issues with some packages. In case you encounter those, try switching to source mode.

#### GIT plugin
The git/github plugin supports any git repository. The urls are the package names, and the tags the versions. Tags must be real versions (e.g. `1.0.1`). It cannot search, and may not support all url schemes. It also comes with a github provider, which can be used to have simpler names for github projects.

## Installation
- Arch-Users: [qpmx](https://aur.archlinux.org/packages/qpmx/), [qpmx-gitsource](https://aur.archlinux.org/packages/qpmx-gitsource/), [qpmx-qpmsource](https://aur.archlinux.org/packages/qpmx-qpmsource/)
- Other package managers are planned (deb, rpm, brew, chocolaty, ...)
- Compile yourself. You will need [QtJsonSerializer](https://github.com/Skycoder42/QJsonSerializer)
- Download from the releases package

## Examples
Have a look at https://github.com/Skycoder42/qpmx-sample-package. Its a project with a sample qpmx packages, as well as an application (https://github.com/Skycoder42/qpmx-sample-package/tree/master/qpmx-test) that uses this sample package. It's a git/github package with dependencies to qpm packages, to show off the possibilities.

### General usage
Simply install packages using `qpmx install` (If not done automatically, prepare the pro file with `qpm init --prepare <pro-file>`). And thats it. Simply run qmake, and qpmx will automatically install, precompile and prepare
packages, and include everything required to your pro file automatically.

#### Translations
To have translations properly working, you can use the `TRANSLATIONS` variable in both, qpmx packages and in your final project. Simply run `make lrelease` and translations will be compiled automatically. The same happens with files in `EXTRA_TRANSLATIONS`, but without joining them with qpmx package translations. You can use that one if you have more the 1 ts file per language. To install them, use the prepared target:

```pro
qpmx_ts_target.path = $$[QT_INSTALL_TRANSLATIONS] # or wherever you want to install them to
INSTALLS += qpmx_ts_target
```

### Common use cases
#### Package Users
- Installing a package: `qpmx install [<provider>::]<package>[@<version>]`
Example: `qpmx install com.github.skycoder42.qpmx-sample-package` would search all providers for the package and then install the latest version.
- Preparing a pro-file to include qpmx packages: `qpmx init --prepare <pro-file>`
This is done automatically on the first install, but if you are missing the line, you can add it this way.
- Search for a package `qpmx search de.skycoder42.qtmvvm`
Will search all providers that support searching (qpm) for packages that match the given name.

#### Package Developers
- Create a qpmx-file for a package: `qpmx create --prepare qpm`
This will create/update a qpmx-file based of your inputs, and in this example, prepare it for publishing with the given provider.
- Publish a qpmx package: `qpmx publish 4.2.0`
Publishes the package for all providers it was prepared for, with the given version of 4.2.0
- Switch a package dependency to a local path (dev mode): `qpmx dev add <provider>::<package>@<version> <path>`
This will use the local `<path>` as package source instead of downloading it from git/qpm/... (of course only for this qpmx.json project)

## Special (qmake) stuff
### qmake variables
 Variable						| Description
--------------------------------|-------------
QPMX_EXTRA_OPTIONS				| Additional option parameters for the `qpmx init` invocation
QPMX_TRANSLATE_EXTRA_OPTIONS	| Additional option parameters for the `qpmx translate` invocation
QPMX_HOOK_EXTRA_OPTIONS			| Additional option parameters for the `qpmx hook` invocation
PUBLIC_HEADERS					| *qpmx package only:* The headers to be used by users. If left empty, `HEADERS` is used
QPMX_WORKINGDIR					| The (sub)directory to use for generation of qpmx files. If left empty, the build directory is used
EXTRA_TRANSLATIONS				| Just like `TRANSLATIONS`, but qpmx will not join those files with the qpmx translations (but still compile)

### Extra targets
 Target			| Description
----------------|-------------
qpmx_ts_target	| A target to install compiled translations (`.qm`) files. Use like the `target` target (See https://doc.qt.io/qt-5/qmake-advanced-usage.html#installing-files)

### Special CONFIG values
 Value			| Description
----------------|-------------
qpmx_static		| *qpmx package only:* Is defined when a qpmx package is build as static library
qpmx_src_build	| *qpmx package only:* Is defined when a qpmx package is included as source package into a project

**Note:** If neither is defined, the package is used as static library in a project (typically, in your prc files)

## Documentation
Planned for the future. You can run `qpmx --help` and `qpmx <command> --help` to see what the tool can do. it's mostly non-interactive, but a few commands do require user interaction.
