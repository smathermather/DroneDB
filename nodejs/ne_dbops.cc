/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#include <iostream>
#include <sstream>
#include "ddb.h"
#include "ne_dbops.h"
#include "ne_helpers.h"

class InitWorker : public Nan::AsyncWorker {
 public:
  InitWorker(Nan::Callback *callback, const std::string &directory)
    : AsyncWorker(callback, "nan:InitWorker"),
      directory(directory) {}
  ~InitWorker() {}

  void Execute () {
    char *outPath;

    if (DDBInit(directory.c_str(), &outPath) != DDBERR_NONE){
      SetErrorMessage(DDBGetLastError());
    }

    ddbPath = std::string(outPath);
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         Nan::New(ddbPath).ToLocalChecked()
     };
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string directory;
    std::string ddbPath;
};


NAN_METHOD(init) {
    ASSERT_NUM_PARAMS(2);

    BIND_STRING_PARAM(directory, 0);
    BIND_FUNCTION_PARAM(callback, 1);

    Nan::AsyncQueueWorker(new InitWorker(callback, directory));
}


class AddWorker : public Nan::AsyncWorker {
 public:
  AddWorker(Nan::Callback *callback, const std::string &ddbPath, const std::vector<std::string> &paths, bool recursive)
    : AsyncWorker(callback, "nan:AddWorker"),
      ddbPath(ddbPath), paths(paths), recursive(recursive) {}
  ~AddWorker() {}

  void Execute () {
      std::vector<const char *> cPaths(paths.size());
      std::transform(paths.begin(), paths.end(), cPaths.begin(), [](const std::string& s) { return s.c_str(); });

      if (DDBAdd(ddbPath.c_str(), cPaths.data(), static_cast<int>(cPaths.size()), &output, recursive) != DDBERR_NONE){
          SetErrorMessage(DDBGetLastError());
      }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     Nan::JSON json;
     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         json.Parse(Nan::New<v8::String>(output).ToLocalChecked()).ToLocalChecked()
     };
     delete output; // TODO: is this a leak if the call fails? How do we de-allocate on failure?
     callback->Call(2, argv, async_resource);
   }

 private:
    char *output;

    std::string ddbPath;
    std::vector<std::string> paths;
    bool recursive;
};


NAN_METHOD(add) {
    ASSERT_NUM_PARAMS(4);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_STRING_ARRAY_PARAM(paths, 1);

    BIND_OBJECT_PARAM(obj, 2);
    BIND_OBJECT_VAR(obj, bool, recursive, false);

    BIND_FUNCTION_PARAM(callback, 3);

    Nan::AsyncQueueWorker(new AddWorker(callback, ddbPath, paths, recursive));
}


class RemoveWorker : public Nan::AsyncWorker {
 public:
  RemoveWorker(Nan::Callback *callback, const std::string &ddbPath, const std::vector<std::string> &paths)
    : AsyncWorker(callback, "nan:RemoveWorker"),
      ddbPath(ddbPath), paths(paths) {}
  ~RemoveWorker() {}

  void Execute () {
      std::vector<const char *> cPaths(paths.size());
      std::transform(paths.begin(), paths.end(), cPaths.begin(), [](const std::string& s) { return s.c_str(); });

      if (DDBRemove(ddbPath.c_str(), cPaths.data(), static_cast<int>(cPaths.size())) != DDBERR_NONE){
          SetErrorMessage(DDBGetLastError());
      }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         Nan::True()
     };
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string ddbPath;
    std::vector<std::string> paths;    
};


NAN_METHOD(remove) {
    ASSERT_NUM_PARAMS(4);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_STRING_ARRAY_PARAM(paths, 1);

    // TODO: this is not needed?
    //BIND_OBJECT_PARAM(obj, 2);

    BIND_FUNCTION_PARAM(callback, 3);

    Nan::AsyncQueueWorker(new RemoveWorker(callback, ddbPath, paths));
}

class MoveWorker : public Nan::AsyncWorker {
 public:
  MoveWorker(Nan::Callback *callback, const std::string &ddbPath, const std::string &source, const std::string &dest)
    : AsyncWorker(callback, "nan:MoveWorker"),
      ddbPath(ddbPath), source(source), dest(dest) {}
  ~MoveWorker() {}

  void Execute () {
      if (DDBMoveEntry(ddbPath.c_str(), source.c_str(), dest.c_str()) != DDBERR_NONE){
          SetErrorMessage(DDBGetLastError());
      }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         Nan::True()
     };
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string ddbPath;
    std::string source;
    std::string dest;
};


NAN_METHOD(move) {
    ASSERT_NUM_PARAMS(4);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_STRING_PARAM(source, 1);
    BIND_STRING_PARAM(dest, 2);
    BIND_FUNCTION_PARAM(callback, 3);

    Nan::AsyncQueueWorker(new MoveWorker(callback, ddbPath, source, dest));
}

class ListWorker : public Nan::AsyncWorker {
 public:
  ListWorker(Nan::Callback *callback, const std::string &ddbPath, const std::vector<std::string> &paths, 
     bool recursive, int maxRecursionDepth)
    : AsyncWorker(callback, "nan:ListWorker"),
      ddbPath(ddbPath), paths(paths), recursive(recursive), maxRecursionDepth(maxRecursionDepth) {}
  ~ListWorker() {}

  void Execute () {
         
    std::vector<const char *> cPaths(paths.size());
    std::transform(paths.begin(), paths.end(), cPaths.begin(), [](const std::string& s) { return s.c_str(); });

    if (DDBList(ddbPath.c_str(), cPaths.data(), static_cast<int>(cPaths.size()), &output, "json", recursive, maxRecursionDepth) != DDBERR_NONE){
        SetErrorMessage(DDBGetLastError());
    }

  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     Nan::JSON json;
     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         json.Parse(Nan::New<v8::String>(output).ToLocalChecked()).ToLocalChecked()
     };

     delete output; // TODO: is this a leak if the call fails? How do we de-allocate on failure?
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string ddbPath;
    std::vector<std::string> paths;

    bool recursive;
    int maxRecursionDepth;   
    char *output;

};

NAN_METHOD(list) {
    ASSERT_NUM_PARAMS(4);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_STRING_ARRAY_PARAM(in, 1);
    BIND_OBJECT_PARAM(obj, 2);
    BIND_OBJECT_VAR(obj, bool, recursive, false);
    BIND_OBJECT_VAR(obj, int, maxRecursionDepth, 0);
    BIND_FUNCTION_PARAM(callback, 3);

    Nan::AsyncQueueWorker(new ListWorker(callback, ddbPath, in, recursive, maxRecursionDepth));
}

class BuildWorker : public Nan::AsyncWorker {
 public:
  BuildWorker(Nan::Callback *callback, const std::string &ddbPath, const std::string &path,
    bool force, bool pendingOnly)
    : AsyncWorker(callback, "nan:BuildWorker"),
      ddbPath(ddbPath), path(path), force(force), pendingOnly(pendingOnly) {}
  ~BuildWorker() {}

  void Execute () {
    if (DDBBuild(ddbPath.c_str(), path.c_str(), nullptr, force, pendingOnly) != DDBERR_NONE){
      SetErrorMessage(DDBGetLastError());
    }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         Nan::True()
     };
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string ddbPath;
    std::string path;
    bool force;
    bool pendingOnly;
};

NAN_METHOD(build) {
    ASSERT_NUM_PARAMS(3);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_OBJECT_PARAM(obj, 1);

    BIND_OBJECT_STRING(obj, path, "");
    BIND_OBJECT_VAR(obj, bool, force, false);
    BIND_OBJECT_VAR(obj, bool, pendingOnly, false);

    BIND_FUNCTION_PARAM(callback, 2);

    Nan::AsyncQueueWorker(new BuildWorker(callback, ddbPath, path, force, pendingOnly));
}

class SearchWorker : public Nan::AsyncWorker {
 public:
  SearchWorker(Nan::Callback *callback, const std::string &ddbPath, const std::string &query)
    : AsyncWorker(callback, "nan:SearchWorker"),
      ddbPath(ddbPath), query(query) {}
  ~SearchWorker() {}

  void Execute () {
    if (DDBSearch(ddbPath.c_str(), query.c_str(), &output, "json") != DDBERR_NONE){
        SetErrorMessage(DDBGetLastError());
    }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     Nan::JSON json;
     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         json.Parse(Nan::New<v8::String>(output).ToLocalChecked()).ToLocalChecked()
     };

     delete output;
     callback->Call(2, argv, async_resource);
  }

 private:
    std::string ddbPath;
    std::string query;
    char *output;
};

NAN_METHOD(search) {
    ASSERT_NUM_PARAMS(3);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_STRING_PARAM(query, 1);
    BIND_FUNCTION_PARAM(callback, 2);

    Nan::AsyncQueueWorker(new SearchWorker(callback, ddbPath, query));
}


class ChattrWorker : public Nan::AsyncWorker {
 public:
  ChattrWorker(Nan::Callback *callback, const std::string &ddbPath, const std::string &attrsJson)
    : AsyncWorker(callback, "nan:ChattrWorker"),
      ddbPath(ddbPath), attrsJson(attrsJson){}
  ~ChattrWorker() {}

  void Execute () {
    if (DDBChattr(ddbPath.c_str(), attrsJson.c_str(), &output) != DDBERR_NONE){
        SetErrorMessage(DDBGetLastError());
    }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     Nan::JSON json;
     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         json.Parse(Nan::New<v8::String>(output).ToLocalChecked()).ToLocalChecked()
     };

     callback->Call(2, argv, async_resource);
   }

 private:
    std::string ddbPath;
    std::string attrsJson;

    char *output;
};

NAN_METHOD(chattr) {
    ASSERT_NUM_PARAMS(3);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_OBJECT_PARAM(attrs, 1);

    Nan::JSON NanJSON;
    Nan::MaybeLocal<v8::String> result = NanJSON.Stringify(attrs);
    std::string attrsJson;

    if (!result.IsEmpty()) {
        Nan::Utf8String str(result.ToLocalChecked().As<v8::String>());
        attrsJson = std::string(*str);
    }

    BIND_FUNCTION_PARAM(callback, 2);

    Nan::AsyncQueueWorker(new ChattrWorker(callback, ddbPath, attrsJson));
}

class GetWorker : public Nan::AsyncWorker {
 public:
  GetWorker(Nan::Callback *callback, const std::string &ddbPath, const std::string &path)
    : AsyncWorker(callback, "nan:GetWorker"),
      ddbPath(ddbPath), path(path) {}
  ~GetWorker() {}

  void Execute () {
    if (DDBGet(ddbPath.c_str(), path.c_str(), &output) != DDBERR_NONE){
        SetErrorMessage(DDBGetLastError());
    }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     Nan::JSON json;
     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         json.Parse(Nan::New<v8::String>(output).ToLocalChecked()).ToLocalChecked()
     };

     delete output; // TODO: is this a leak if the call fails? How do we de-allocate on failure?
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string ddbPath;
    std::string path;
 
    char *output;

};

NAN_METHOD(get) {
    ASSERT_NUM_PARAMS(3);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_STRING_PARAM(path, 1);
    BIND_FUNCTION_PARAM(callback, 2);

    Nan::AsyncQueueWorker(new GetWorker(callback, ddbPath, path));
}

class GetStampWorker : public Nan::AsyncWorker {
 public:
  GetStampWorker(Nan::Callback *callback, const std::string &ddbPath)
    : AsyncWorker(callback, "nan:GetStampWorker"),
      ddbPath(ddbPath) {}
  ~GetStampWorker() {}

  void Execute () {
    if (DDBGetStamp(ddbPath.c_str(), &output) != DDBERR_NONE){
        SetErrorMessage(DDBGetLastError());
    }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     Nan::JSON json;
     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         json.Parse(Nan::New<v8::String>(output).ToLocalChecked()).ToLocalChecked()
     };

     delete output; // TODO: is this a leak if the call fails? How do we de-allocate on failure?
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string ddbPath;
    char *output;

};

NAN_METHOD(getStamp) {
    ASSERT_NUM_PARAMS(2);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_FUNCTION_PARAM(callback, 1);

    Nan::AsyncQueueWorker(new GetStampWorker(callback, ddbPath));
}

class DeltaWorker : public Nan::AsyncWorker {
 public:
  DeltaWorker(Nan::Callback *callback, const std::string &sourceStamp, const std::string &targetStamp)
    : AsyncWorker(callback, "nan:DeltaWorker"),
      sourceStamp(sourceStamp), targetStamp(targetStamp) {}
  ~DeltaWorker() {}

  void Execute () {
    if (DDBDelta(sourceStamp.c_str(), targetStamp.c_str(), &output, "json") != DDBERR_NONE){
        SetErrorMessage(DDBGetLastError());
    }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     Nan::JSON json;
     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         json.Parse(Nan::New<v8::String>(output).ToLocalChecked()).ToLocalChecked()
     };

     delete output; // TODO: is this a leak if the call fails? How do we de-allocate on failure?
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string sourceStamp;
    std::string targetStamp;
    char *output;

};

NAN_METHOD(delta) {
    ASSERT_NUM_PARAMS(3);

    BIND_STRING_PARAM(sourceStamp, 0);
    BIND_STRING_PARAM(targetStamp, 1);
    
    BIND_FUNCTION_PARAM(callback, 2);

    Nan::AsyncQueueWorker(new DeltaWorker(callback, sourceStamp, targetStamp));
}

class ComputeDeltaLocalsWorker : public Nan::AsyncWorker {
 public:
  ComputeDeltaLocalsWorker(Nan::Callback *callback, const std::string &ddbPath, const std::string &delta, const std::string &hlDestFolder)
    : AsyncWorker(callback, "nan:ComputeDeltaLocalsWorker"),
      ddbPath(ddbPath), delta(delta), hlDestFolder(hlDestFolder) {}
  ~ComputeDeltaLocalsWorker() {}

  void Execute () {
    if (DDBComputeDeltaLocals(delta.c_str(), ddbPath.c_str(), hlDestFolder.c_str(), &output) != DDBERR_NONE){
        SetErrorMessage(DDBGetLastError());
    }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     Nan::JSON json;
     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         json.Parse(Nan::New<v8::String>(output).ToLocalChecked()).ToLocalChecked()
     };

     delete output; // TODO: is this a leak if the call fails? How do we de-allocate on failure?
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string ddbPath;
    std::string delta;
    std::string hlDestFolder;
    char *output;

};

NAN_METHOD(computeDeltaLocals) {
    ASSERT_NUM_PARAMS(4);

    BIND_STRING_PARAM(ddbPath, 0);
    BIND_STRING_PARAM(delta, 1);
    BIND_STRING_PARAM(hlDestFolder, 2);
    
    BIND_FUNCTION_PARAM(callback, 3);

    Nan::AsyncQueueWorker(new ComputeDeltaLocalsWorker(callback, ddbPath, delta, hlDestFolder));
}

class ApplyDeltaWorker : public Nan::AsyncWorker {
 public:
  ApplyDeltaWorker(Nan::Callback *callback, const std::string &delta, const std::string &sourcePath, const std::string &ddbPath,
  int mergeStrategy, const std::string &sourceMetaDump)
    : AsyncWorker(callback, "nan:ApplyDeltaWorker"),
      delta(delta), sourcePath(sourcePath), ddbPath(ddbPath), mergeStrategy(mergeStrategy), sourceMetaDump(sourceMetaDump) {}
  ~ApplyDeltaWorker() {}

  void Execute () {
    if (DDBApplyDelta(delta.c_str(), sourcePath.c_str(), ddbPath.c_str(), mergeStrategy, sourceMetaDump.c_str(), &output) != DDBERR_NONE){
        SetErrorMessage(DDBGetLastError());
    }
  }

  void HandleOKCallback () {
     Nan::HandleScope scope;

     Nan::JSON json;
     v8::Local<v8::Value> argv[] = {
         Nan::Null(),
         json.Parse(Nan::New<v8::String>(output).ToLocalChecked()).ToLocalChecked()
     };

     delete output;
     callback->Call(2, argv, async_resource);
   }

 private:
    std::string delta;
    std::string sourcePath;
    std::string ddbPath;
    int mergeStrategy;
    std::string sourceMetaDump;
    char *output;

};

NAN_METHOD(applyDelta) {
    ASSERT_NUM_PARAMS(6);

    BIND_STRING_PARAM(delta, 0);
    BIND_STRING_PARAM(sourcePath, 1);
    BIND_STRING_PARAM(ddbPath, 2);
    BIND_STRING_PARAM(sourceMetaDump, 3);
    BIND_OBJECT_PARAM(obj, 4);
    BIND_OBJECT_VAR(obj, int, mergeStrategy, 0);
    
    BIND_FUNCTION_PARAM(callback, 5);

    Nan::AsyncQueueWorker(new ApplyDeltaWorker(callback, delta, sourcePath, ddbPath, mergeStrategy, sourceMetaDump));
}