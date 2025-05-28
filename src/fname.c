
#include "fname.h"
#include <time.h>
#include <stdio.h>
#include <windows.h>
// #include <winsock.h>


#define FNAME_VAR_LEN 32
#define FNAME_MAX 256

fname_var_2_val_fn_t *fname_var_2_val_fn=0;
int fname_errmsg_l=FNAME_MAX;
char fname_errmsg[FNAME_MAX];

char *fname_hostname_p;

void fname_init(fname_var_2_val_fn_t *var_2_val_fn) {
  fname_var_2_val_fn=var_2_val_fn;
  //  fname_errmsg = errmsg;
  //  fname_errmsg_l = errmsg_len; // TODO: currently this is ignored!
}

char *fname_get_errmsg(void) {
  return fname_errmsg;
}

int fname_is_fsep(char c) {
  return ((c=='/')||(c=='\\'));
}

int fname_is_relative(char *pname) {
  if (!pname[0]) return 1;
  if ((pname[0]=='/')||(pname[0]=='\\')) return 0;
  if (pname[1]==':') return 0; // drive style is  C:/blablah
  return 1;
}

void fname_ensure_trailing_fsep(char *s, int s_len) {
  if (!fname_is_fsep(s[strlen(s)-1])) strcat_s(s, s_len, "\\");
}

void fname_concat(char *pname, int pname_max, char *rel_fname) {
  // concatinates a relative filename (or pathname) to an existing pathname
  int l;
  char *s;
  s = rel_fname;
  if (   fname_is_fsep(s[0])
      || ((pname[0]=='.')&&(!pname[1]))
      || (s[0]&&(s[1]==':'))) { // absolute
    strcpy_s(pname, pname_max, s);
    return;
  }
  fname_ensure_trailing_fsep(pname, pname_max);
  l = strlen(pname);
  while(1) {
    if ((s[0]=='.')&&(s[1]=='.')) {
      if (l>1) { // TODO: there is a bug here
	for(--l;(l>0)&&!fname_is_fsep(pname[l-1]);--l);
	pname[l]=0;
      }
      s+=2;
      if (fname_is_fsep(*s)) ++s;
    }else break;
  }
  strcat_s(pname, pname_max, s);
}

int fname_last_fsep(char *fname) {
  // returns -1 if not found
  int i;
  for(i=strlen(fname)-1; i>=0; --i)
    if (fname_is_fsep(fname[i])) break;
  return i;
}

void fname_pathof(char *pname, int pname_max, char *full_fname) {
  int i;
  i = fname_last_fsep(full_fname);
  if (i>=0) {
    i=min(pname_max-1,i);
    strncpy_s(pname, pname_max, full_fname, i);
  }else {
    pname[0]='.';
    pname[1]=0;
  }
}



int fname_expand_l(char *dst, int dst_len, char *src,
		   int *has, int unum) {
  // desc: expands a filename in src and puts result in dst
  //   unum: number to use for ${NUM}
  //   has: set to relevant mask of FNAME_HAS_*
  //   on error, also sets fname_errmsg
  int i, j, di,e;
  char tok[FNAME_VAR_LEN];
  int last_fsep;
  di=0;
  *has=0;
  last_fsep = fname_last_fsep(src);
  dst[0]=0;
  for(i=0;src[i];++i) {
    if (src[i]=='$') {
      ++i;
      if (src[i]=='{') {
	++i;
	for(j=0; src[i]&&(src[i]!='}');)
	  tok[j++]=src[i++];
	if (src[i]!='}') {
	  sprintf_s(fname_errmsg, FNAME_MAX, "unterminated bracket");
	  return ERR_SYNTAX;
	}
	tok[j]=0;

	// printf("DBG: src %s  at %d  tok %s len %d\n", src, i, tok, j);
	if (!_stricmp(tok,"host")) {
	  sprintf_s(dst+di, dst_len-di, "%s", fname_hostname_p);
	  di+=strlen(fname_hostname_p);
	}else if (!_stricmp(tok,"date")) {
	  time_t tt;
	  struct tm *nt;
	  time(&tt);
	  nt = localtime(&tt);
	  sprintf_s(dst+di, dst_len-di, "%02d%02d%02d", (nt->tm_year+1900)%100, 
		  nt->tm_mon+1, nt->tm_mday);
	  di+=strlen(dst+di);
	}else if (!_stricmp(tok,"num")) {
	  if (i>last_fsep) {
	    *has |= FNAME_HAS_NUM;
	    sprintf_s(dst+di,  dst_len-di, "%03d", unum);
	    di+=strlen(dst+di);
	  }else {
	    sprintf_s(fname_errmsg, FNAME_MAX, "${NUM} file variable not allowed in path");
	    return ERR_SYNTAX;
	  }
	}else if (fname_var_2_val_fn) {
	  e=fname_var_2_val_fn(dst+di, dst_len-di, tok, has);
	  if (!e) di+=strlen(dst+di);
	  if (e) {
	    if (e==2) sprintf_s(fname_errmsg, FNAME_MAX, "${%s} file variable undefined", tok);
            else sprintf_s(fname_errmsg, FNAME_MAX, "${%s} failed", tok);
	    return ERR_FAIL;
	  }
	}else {
	  sprintf_s(fname_errmsg, FNAME_MAX, "${%s} file variable undefined", tok);
	  return ERR_FAIL;
	}

      }else {
	sprintf_s(fname_errmsg, FNAME_MAX, "bad char after $");
	return ERR_SYNTAX;
      }
    }else dst[di++]=src[i];
  }
  dst[di]=0;
  return 0;
}


int fname_expand(char *fname, int fname_len,
		 int *has_p, int *unum_p) {
  // dst pre-loaded with path and file sep.
  //   has_p: set to or of FNAME_HAS_*
  //   unum_p: if non-null, set to value of ${NUM} found
  char tmp[FNAME_MAX];
  int e, unum, unique=0, has;
  HANDLE h;
  if (strlen(fname)>=FNAME_MAX) {
    sprintf_s(fname_errmsg, FNAME_MAX, "BUG: fname too long");
    return ERR_FAIL;
  }
  strcpy_s(tmp, FNAME_MAX, fname);

  for(unum=0;unum<1000;++unum) {
    e=fname_expand_l(fname, fname_len, tmp, &has, unum);
    if (e) return e;
    if (!(has&FNAME_HAS_NUM)) break;
    h = CreateFile(fname, GENERIC_READ | GENERIC_WRITE,
		   FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    unique=(h == INVALID_HANDLE_VALUE);
    if (unique) break;
    CloseHandle(h);
  }
  if (unum_p) *unum_p = unum;
  if (has_p) *has_p = has;
  if (!(has&FNAME_HAS_NUM) || unique) return 0;

  sprintf_s(fname_errmsg, FNAME_MAX, "no unique ${NUM} left");
  return ERR_FAIL;
}


char *fname_ext(char *fname) {
  // returns ptr to extension, or 0 if none
  char *p;
  p=strrchr(fname,'.');
  if (!p || (p==fname)) return 0;
  if (*(p-1)=='.') return 0;
  return p;
}

int fname_exists(char *path, int *exists) {
  HANDLE h;
  int i;
  h = CreateFile(path, 0, // GENERIC_READ,
		   FILE_SHARE_READ,
		   0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h == INVALID_HANDLE_VALUE) {
    // printf("invalid path %s\n", path);
    *exists=0;
    i=GetLastError();
    if ((i==ERROR_PATH_NOT_FOUND)||(i==ERROR_FILE_NOT_FOUND)) {// "normal" errors
      return 0;
    }
    printf("ERR: fname_exists() failed\n");
    printf("     windows err %d x%x\n", i, i);
    return ERR_FAIL;
  }
  *exists=1;
  CloseHandle(h);
  return 0;
}

int file_ensure_path(char *path) {
  HANDLE h;
  int i, j;

  for (j=0;j<2;++j) {
    h = CreateFile(path, 0, // GENERIC_READ,
		   FILE_SHARE_READ,
		   0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
    if (h == INVALID_HANDLE_VALUE) {
      // printf("invalid path %s\n", path);
      i=GetLastError();
      if (!j && (i==ERROR_PATH_NOT_FOUND)) {
	char pathof[FNAME_MAX];
	// printf("  no path\n");
	fname_pathof(pathof, FNAME_MAX, path);
	i=file_ensure_path(pathof);
	if (i) return i;
	continue;
      }
      if (i==ERROR_FILE_NOT_FOUND) {
	// printf("  no file\n");
	// this is only able to create the final dir in the path
	if (!CreateDirectory(path, 0)) {
	  i=GetLastError();
	  if ((i!=ERROR_ALREADY_EXISTS)&&(i!=ERROR_FILE_EXISTS)) {
	    printf("ERR: failed to create dir %s\n", path);
	    printf("     windows err %d x%x\n", i, i);
	    return ERR_FAIL;
	  }
	}
	break;
      }else {
	printf("NOTE: ensure_dir_exists() failed\n");
	printf("     windows err %d x%x\n", i, i);
	return ERR_FAIL;
      }
    }else {
      // printf("path %s ok\n", path);
      CloseHandle(h);
      break;
    }
  }
  return 0;
}


int fname_ensure_dir_exists(char *fname) {
  //  HANDLE h;
  char fn[FNAME_MAX];
  //  int i;
  fname_pathof(fn, FNAME_MAX, fname);
  return file_ensure_path(fn);
  /*
  h = CreateFile(fn, 0, // GENERIC_READ,
		 FILE_SHARE_READ,
		 0, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, 0);
  if (h == INVALID_HANDLE_VALUE) {
    printf("inv path\n");
    i=GetLastError();
    if ((i==ERROR_FILE_NOT_FOUND)||(i==ERROR_PATH_NOT_FOUND)) {
      printf("%s no fnd\n", fn);
      if (!SHCreateDirectoryEx(0,fn, 0)) {
	i=GetLastError();
	if ((i!=ERROR_ALREADY_EXISTS)&&(i!=ERROR_FILE_EXISTS)) {
	  printf("ERR: failed to create dir %s\n", fn);
	  printf("     windows err %d x%x\n", i, i);
	  return ERR_FAIL;
	}
      }
    } else {
      printf("NOTE: ensure_dir_exists() failed\n");
      printf("     windows err %d x%x\n", i, i);
    }
  }else {
    // printf("path %s ok\n", fn);
    CloseHandle(h);
  }
  return 0;
  */
}
