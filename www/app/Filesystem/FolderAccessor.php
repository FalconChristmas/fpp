<?php

namespace FPP\Filesystem;

use FPP\API\File;
use FPP\API\Path;
use FPP\API\Helper;

/**
 * Provides methods to manipulate folders
 */
class FolderAccessor
{
    private $disk;
    private $filesystem;

    public function __construct($disk, \Illuminate\Contracts\Filesystem\Filesystem $filesystem)
    {
        $this->disk = $disk;
        $this->filesystem = $filesystem;
    }

    /**
     * @return \Illuminate\Contracts\Filesystem\Filesystem
     */
    protected function fs()
    {
        return $this->filesystem;
    }

    /**
     * Does a folder exist?
     *
     * @param string $folder Path to folder
     * @return bool
     */
    public function exists($folder)
    {
        return $this->fs()->exists(Path::makeRelative($folder));
    }

    /**
     * Make a directory
     *
     * @param string $folder Path to folder
     * @return bool
     */
    public function make($folder)
    {
        $folder = Path::makeRelative($folder);

        if ($this->exists($folder)) {
            return true;
        }

        return $this->fs()->makeDirectory($folder);
    }

    /**
     * Get files from a folder
     *
     * @param string $folder     Path to folder
     * @param bool   $recursive  Whether to look recursively
     * @return array
     */
    public function getFiles($folder, $recursive = false)
    {
        return $this->fs()->files(Path::makeRelative($folder), $recursive);
    }

    /**
     * Get files from a folder, recursively
     *
     * @param string $folder  Path to folder
     * @return array
     */
    public function getFilesRecursively($folder)
    {
        return $this->getFiles($folder, true);
    }

    /**
     * Get files from a folder and filter them by type/extension
     *
     * @param string       $folder     Path to folder
     * @param array|string $extension  Extension or extensions
     * @param bool         $recursive  Whether to look recursively
     * @return array
     */
    public function getFilesByType($folder, $extension, $recursive = false)
    {
        $files = collect_files($this->getFiles($folder, $recursive));

        $extensions = Helper::ensureArray($extension);

        return $files->filterByExtension($extensions)->all();
    }

    /**
     * Get files from a folder and filter them by type/extension, recursively
     *
     * @param string       $folder     Path to folder
     * @param array|string $extension  Extension or extensions
     * @return array
     */
    public function getFilesByTypeRecursively($folder, $extension)
    {
        return $this->getFilesByType($folder, $extension);
    }

    /**
     * Get subfolders in a folder
     *
     * @param string $folder     Path to folder
     * @param bool   $recursive  Whether to look recursively
     * @return array
     */
    public function getFolders($folder, $recursive = false)
    {
        return $this->fs()->directories(Path::makeRelative($folder), $recursive);
    }

    /**
     * Get subfolders in a folder, recursively
     *
     * @param string $folder Path to folder
     * @return array
     */
    public function getFoldersRecursively($folder)
    {
        return $this->getFolders($folder, true);
    }

    /**
     * Get the last modified timestamp
     *
     * @param string $folder  Path to folder
     * @return mixed
     */
    public function getLastModified($folder)
    {
        return $this->fs()->lastModified(Path::makeRelative($folder));
    }

    /**
     * Is a given folder empty?
     *
     * @param $folder
     * @return bool
     */
    public function isEmpty($folder)
    {
        $files = $this->getFilesRecursively($folder);

        return empty($files);
    }

    /**
     * Copy a folder
     *
     * @param string $src  Path to the source folder
     * @param string $dest Path to the destination folder
     * @param bool   $overwrite Should the destination get overwritten if it exists?
     */
    public function copy($src, $dest, $overwrite = false)
    {
        $src  = Path::makeRelative($src);
        $dest = Path::makeRelative($dest);

        $files = $this->getFiles($src, true);

        foreach ($files as $file) {
            $dest_file = preg_replace('#^'.$src.'#', $dest, $file);
            File::disk($this->disk)->copy($file, $dest_file, $overwrite);
        }
    }

    /**
     * Rename a folder
     *
     * @param string $old_folder Existing folder's path
     * @param string $new_folder Path to rename to
     */
    public function rename($old_folder, $new_folder)
    {
        $old_folder = Path::makeRelative($old_folder);
        $new_folder = Path::makeRelative($new_folder);

        // Rename all the files within the folder
        foreach ($this->getFilesRecursively($old_folder) as $old_path) {
            $new_path = preg_replace('#^'. $old_folder .'#', $new_folder, $old_path);
            File::disk($this->disk)->rename($old_path, $new_path);
        }

        // We've moved files, not folders. The empty folder remains, so delete it.
        $this->delete($old_folder);
    }

    /**
     * Delete a folder
     *
     * @param string $folder
     */
    public function delete($folder)
    {
        $folder = Path::makeRelative($folder);

        $this->fs()->deleteDirectory($folder);
    }

    /**
     * Deletes empty subfolders from a folder
     *
     * Local file system adapter only
     *
     * @param string $folder
     */
    public function deleteEmptySubfolders($folder)
    {
        // Only adapters with a concept of folders can use this
        // @todo: Add a conditional here that will bail out for adapters with no folders

        // Grab all the folders
        $folders = $this->getFoldersRecursively($folder);

        // Sort by deepest first. In order to delete a folder, it must be empty.
        // This means we need to delete the deepest child folders first.
        uasort($folders, function($a, $b) {
            return (substr_count($a, '/') >= substr_count($b, '/')) ? -1 : 1;
        });

        // Iterate and delete
        foreach ($folders as $dir) {
            if ($this->isEmpty($dir)) {
                $this->delete($dir);
            }
        }
    }
}
