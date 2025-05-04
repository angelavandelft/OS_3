
static int
edfuse_read(const char *path, char *buf, size_t size, off_t offset,
            struct fuse_file_info *fi)
{
  /* TODO: implement
   *
   * See also Section 4.2 of the Appendices document.
   *
   * Read @size bytes of data from @path starting at @offset and write
   * this to @buf.
   */

   if(size == 0) return 0;

  edfs_image_t *img = get_edfs_image();

  edfs_inode_t inode = { 0, };

  if (!edfs_find_inode(img, path, &inode))
    return -ENOENT;

  if (edfs_disk_inode_is_directory(&inode.inode))
    return -EISDIR;

  bool indirect = (inode.inode.type == EDFS_INODE_TYPE_INDIRECT);

  int blockoffset = 0;
  int startblock = (int) (offset /img->sb.block_size);

  if(startblock > EDFS_INODE_N_BLOCKS){
     printf("invalid offset");
     return -EINVAL;
  }

  if(offset > 0)
    blockoffset = offset - startblock*img->sb.block_size;

  size_t tbr = size;

  if(!indirect){
    char *write_ptr = buf;

    // All entries of the blocks array point to data blocks of the file
    for (int bkentry = 0; bkentry < EDFS_INODE_N_BLOCKS; bkentry++) {
        printf("%d", bkentry);
        if (bkentry < startblock) continue;
        if (inode.inode.blocks[bkentry] == EDFS_BLOCK_INVALID) continue;
        if (tbr <= 0) break;

        int readsize = img->sb.block_size - blockoffset;
        if (readsize > tbr) readsize = tbr;

        uint32_t offset1 = edfs_get_block_offset(&img->sb, inode.inode.blocks[bkentry]);

        if (pread(img->fd, write_ptr, readsize, offset1 + blockoffset) < 0) {
            perror("read error in edfuse_visit_dir");
            return -EIO;
        }

        blockoffset = 0;
        tbr -= readsize;
        write_ptr += readsize;
    }
    return size - tbr;

  }else{
    return -ENOSYS;
    /*
    All entries point to indirect blocks, so in this case there can be
    up to two indirect blocks. An indirect block contains block numbers to data blocks. Within the indirect
    block these numbers are stored consecutively and therefore the contents of an indirect block can be seen
    as a small array of block numbers.
  */
    int blck = 0;
    for(int bkentry = 0; bkentry < EDFS_INODE_N_BLOCKS; bkentry++){

      edfs_block_t* blockbuf = malloc(img->sb.block_size);
      uint32_t offset = edfs_get_block_offset(&img->sb, inode.inode.blocks[bkentry]);

      if(pread(img->fd, blockbuf, img->sb.block_size, offset) <0){
          printf("error loading block_buffer in read");
          free(blockbuf);
          return false;
      }

      for(int indentry = 0; indentry < edfs_get_n_blocks_per_indirect_block(&img->sb); indentry++){

        if (blck+indentry < startblock) continue;
        if(blockbuf[indentry] == EDFS_BLOCK_INVALID){
          free(blockbuf);
          return -ENOENT;
        }

        if (tbr <= 0) {
          free(blockbuf);
          return true;
        }

        //to account for offsets when reading
        int readsize =img->sb.block_size - blockoffset;

        //don't need to read until end
        if (readsize > tbr) readsize = tbr;

        uint32_t offset1 = edfs_get_block_offset(&img->sb, blockbuf[indentry]);

        if(pread(img->fd, buf, readsize, offset1) <0){
            printf("read error in edfuse_visit_dir");
            free(blockbuf);
            return false;
        }
        blockoffset = 0;
        tbr -= readsize;
        }
      free(blockbuf);
      blck += 2;
    }
  }

  return size;
}