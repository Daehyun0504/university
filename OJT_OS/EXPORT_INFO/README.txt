OJT_OS GitHub source export

Original: /home/yoondh/work/01_UNI/OJT_OS
Original files were not deleted or modified. No GitHub upload was performed.

This export retains current tracked source files, non-ignored untracked source
files, custom drivers/DTS/build scripts, and .config/.config.old files.
It is a source snapshot, not a full Git-history or bootable-system backup.
Repository commit IDs and remotes are recorded in repositories.json.
Included file hashes/links are in included-files.json; omissions and reasons
are in excluded-files.json. Build configuration copies under build-configs/
use non-hidden names so upstream .gitignore rules cannot hide them.
Original .config files also remain in their source locations.

Omitted: original archives, nested .git histories, ignored build products,
large Ubuntu rootfs/boot trees, generated images and indexes. Ubuntu install
scripts are retained, but need images from the original or another host.
Not every omitted file can necessarily be regenerated from this export.
Empty directories and device nodes are not represented by normal Git.
Original rootfs remains available for restoring filesystem metadata.

No clean build has been performed. Existing file contents, executable modes
and symbolic links are preserved for included files. Some original tracked
symlinks were already replaced by files/directories; this export preserves
the current working tree, not a checkout of the recorded commit.

To publish from the export directory:
  git init
  # Force-add the curated snapshot once: upstream ignores can hide sources.
  git add -f .
  git status
  git commit -m "Import OJT OS source snapshot"
  # Add your chosen GitHub remote and push after reviewing staged contents.

Do not run git clean -fdx or delete .git in the original workspace.
