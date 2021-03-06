#include "dropbox.h"



/**
*
*/
Dropbox::Dropbox(){

}


/**
*
*/
Dropbox::~Dropbox(){
}


uint32_t Dropbox::authenticate(){
    string strAccessToken;
    filecipher cifrador;
    string accessTokenCipher;
    Dirutil dir;
    bool error=false;
    string display_name, email;
    uint32_t retorno = SINERROR;
    string errorText;

    try{
        Traza::print("rutaini: " + rutaIni, W_DEBUG);
        if (dir.existe(rutaIni)){
            //Obtenemos el access token almacenado en el fichero de configuracion
            ListaIni<Data> *config = new ListaIni<Data>();
            config->loadFromFile(rutaIni);
            config->sort();
            int pos = config->find(DROPBOXACCESSTOKENSTR);
            Data elem = config->get(pos);
            accessTokenCipher = elem.getValue();
            strAccessToken = cifrador.decodeEasy(accessTokenCipher, passwordAT);
            //Comprobamos que podemos obtener info del usuario para saber si el accesstoken es valido
            string AuthOauth2 = "Bearer " + strAccessToken;
            map<string, string> cabeceras;

            cabeceras.clear();
            cabeceras.insert( make_pair("Authorization", AuthOauth2));
            cabeceras.insert( make_pair("Accept", "*/*"));
            cabeceras.insert( make_pair("Accept-Encoding", "deflate"));
            cabeceras.insert( make_pair("Accept-Language", "es-ES,es;q=0.8,en;q=0.6,fr;q=0.4,zh-CN;q=0.2,zh;q=0.2,gl;q=0.2"));
            cabeceras.insert( make_pair("Content-Type", "application/json"));
            //DROPBOXAPIV2
            Json::Value postMsg; //Necesario para enviar algun dato a null en el post en formato json
            util.post(DROPBOXACCOUNTINFO, postMsg.toStyledString(), &cabeceras);
            //DROPBOXAPIV2
            Json::Value root;   // will contains the root value after parsing.
            Json::Reader reader; 
            bool parsingSuccessful = false;
            
            Traza::print("util.getHttp_code()", util.getHttp_code(), W_DEBUG);
            if (util.getHttp_code() != 200){
                error = true;
                retorno = ERRORCONNECT;
            } else {
                parsingSuccessful = reader.parse( util.getData(), root );
            }
            
            if ( !parsingSuccessful ){
                 // report to the user the failure and their locations in the document.
                Traza::print("Dropbox::authenticate: Failed to parse configuration. " + reader.getFormattedErrorMessages(), W_ERROR);
                error = true;
                retorno = ERRORCONNECT;
            } else {
                errorText = root.get("error","").asString();
                display_name = root.get("display_name","").asString();
                email = root.get("email","").asString();
                error = !errorText.empty();
            }
        } else {
            Traza::print("Dropbox::authenticate: No se ha encontrado el fichero de autorizacion", W_ERROR);
            error = true;
            retorno = ERRORACCESSTOKEN;
        }
        //Comprobamos que el access token funciona
        //Si hemos detectado error lanzamos el proceso de autorizacion
        if (error){
            strAccessToken = "";
            if (errorText.find("OAuth 2 \"Authorization\" header is not well-formed") != string::npos
                || errorText.find("Invalid OAuth2 token") != string::npos)
            {
                retorno = ERRORACCESSTOKEN;
            } else if (retorno == SINERROR) {
                //Si no hemos conseguido identificar el error, especificamos error de conexion
                retorno = ERRORCONNECT;
                Traza::print("Dropbox::authenticate: Error: " + errorText, W_ERROR);
            }
        } else {
            Traza::print("Usuario " + display_name + " con correo " + email + " autenticado con exito.", W_DEBUG);
        }
    } catch (Excepcion &e){
        Traza::print("Error al cargar la configuracion", W_ERROR);
        retorno = ERRORACCESSTOKEN;
    }

    this->setAccessToken(strAccessToken);
    setOauthStatus(retorno);
    return retorno;
}

/**
*
*/
string Dropbox::storeAccessToken(string clientid, string secret, string codeOrRefreshToken, bool refresh){
    filecipher cifrador;
    Traza::print("Negociando access token...", W_DEBUG);
    launchAccessToken(clientid, secret, codeOrRefreshToken, refresh);
    string accessTokenCipherB64 = cifrador.encodeEasy(this->getAccessToken(), passwordAT);
    string datos = DROPBOXACCESSTOKENSTR + "=" + accessTokenCipherB64;
    ListaIni<Data> *config = new ListaIni<Data>();
    try{
        Dirutil dir;
        if (dir.existe(rutaIni)){
            config->loadFromFile(rutaIni);
            config->sort();
        }
        this->addToken(DROPBOXACCESSTOKENSTR, accessTokenCipherB64, config);
        config->writeToFile(rutaIni);

    } catch (Excepcion &e){
        Traza::print("GoogleDrive::storeAccessToken. Error al cargar la configuracion", W_ERROR);
    }

    return this->getAccessToken();
}

/**
* lanza la autorizacion desde el explorador para obtener un code
*/
void Dropbox::launchAuthorize(string clientid){
    string tmpUrl = DROPBOXURLAUTH + "?response_type=code&client_id=" + clientid;
    //string cmd = CMD_LAUNCH_BROWSER + " \"" + tmpUrl + "\"";
    //system(cmd.c_str());
    openExplorer(tmpUrl);
}

/**
* Obtiene el access token para hacer subsiguientes peticiones
*/
string Dropbox::launchAccessToken(string clientid, string secret, string code, bool refresh){
    string url = DROPBOXURLTOKEN;
    string s = clientid + ":" + secret;
    string authenticationBasic = "Basic " + Constant::base64_encode(reinterpret_cast<const unsigned char*>(s.c_str()), s.length());
    map<string, string> cabeceras;
    string postData;

    cabeceras.clear();
    cabeceras.insert( make_pair("Authorization", authenticationBasic));
    cabeceras.insert( make_pair("Accept", "*/*"));
    cabeceras.insert( make_pair("Accept-Encoding", "deflate"));
    cabeceras.insert( make_pair("Accept-Language", "es-ES,es;q=0.8,en;q=0.6,fr;q=0.4,zh-CN;q=0.2,zh;q=0.2,gl;q=0.2"));
    cabeceras.insert( make_pair("Content-Type", "application/x-www-form-urlencoded"));
    postData="code=" + code + "&grant_type=authorization_code";
    util.post(url, postData.c_str(),postData.length(), &cabeceras);

    Json::Value root;   // will contains the root value after parsing.
    Json::Reader reader;
    
    string datos = util.getData();
    Traza::print(datos, W_DEBUG);
    
    bool parsingSuccessful = reader.parse( util.getData(), root );
    if ( !parsingSuccessful ){
         // report to the user the failure and their locations in the document.
        Traza::print("Dropbox::launchAccessToken: Failed to parse configuration. " + reader.getFormattedErrorMessages(), W_ERROR);
    } else {
        // Get the value of the member of root named 'hash', return '' if there is no
        // such member.
        accessToken = root.get("access_token","").asString();
    }

    return accessToken;
}

/**
* Sube un fichero con la limitacion de que sea menor a 150MB
*/
int Dropbox::putFile(string filesystemPath, string dropboxPath, string accessToken){
    map<string, string> cabeceras;
    string postData;

    string url = DROPBOXURLPUT + dropboxPath;
    string AuthOauth2 = "Bearer " + accessToken;
    //postData = "Ejemplo automatizado de generacion del token";
    size_t tam;
    char * memblock;
    ifstream file (filesystemPath.c_str(), ios::in|ios::binary|ios::ate);
    std::string test;
    if (file.is_open()){
        tam = file.tellg();
        memblock = new char [tam];
        file.seekg (0, ios::beg);
        file.read (memblock, tam);
        Traza::print("the entire file content is in memory. Size=" + tam, W_DEBUG);

        for(size_t i=0; i < tam -1; i++){
            test = test + memblock[i];
        }
        test += '\0';
        file.close();
    }
    else {
        Traza::print("Unable to open file", W_DEBUG);;
    }

    cabeceras.clear();
    cabeceras.insert( make_pair("Authorization", AuthOauth2));
    cabeceras.insert( make_pair("Accept", "*/*"));
    cabeceras.insert( make_pair("Accept-Encoding", "deflate"));
    cabeceras.insert( make_pair("Accept-Language", "es-ES,es;q=0.8,en;q=0.6,fr;q=0.4,zh-CN;q=0.2,zh;q=0.2,gl;q=0.2"));
    cabeceras.insert( make_pair("Content-Type", "text/plain"));
    cabeceras.insert( make_pair("overwrite", "true"));
    util.post(url,memblock,tam,&cabeceras);
    //cout << util.getData() << endl;
    delete[] memblock;

    return 0;
}

/**
*
*/
bool Dropbox::chunckedUpload(string filesystemPath, string cloudIdPath, string accessToken){
    map<string, string> cabeceras;
    string AuthOauth2 = "Bearer " + accessToken;
    //postData = "Ejemplo automatizado de generacion del token";
    size_t tam;
    string upId = "";
    size_t offsetForDropbox = 0;
    Json::Value root;   // will contains the root value after parsing.
    Json::Reader reader;
    bool ret = false;

    int iteraciones = 0;
    size_t chunkFileSize = 0;
    
    static const size_t DROPBOXCHUNK = 150 * 1024 * 1024; //500KB

    if (!accessToken.empty()){
        ifstream file (filesystemPath.c_str(), ios::in|ios::binary|ios::ate);
        if (file.is_open()){
            tam = file.tellg();
            iteraciones = tam / DROPBOXCHUNK;
        } else {
            Traza::print("Dropbox::chunckedUpload. No se ha podido abrir el fichero: " + filesystemPath, W_ERROR);
            return false;
        }
        file.close();
        
        cabeceras.clear();
        cabeceras.insert( make_pair("Authorization", AuthOauth2));
        //DROPBOXAPIV2
        cabeceras.insert( make_pair("Content-Type", "application/octet-stream"));
        
//        Json::Value postArg;
//        postArg["close"] = false;
//        Json::FastWriter fastWriter;
//        std::string startoutput = fastWriter.write(postArg);
//        Traza::print(startoutput, W_DEBUG);
//        cabeceras.insert( make_pair("Dropbox-API-Arg", startoutput));      
        //util.post(DROPBOXURLPUTSTART, "", &cabeceras);
        
        offsetForDropbox = 0;      
        util.post(DROPBOXURLPUTSTART, filesystemPath.c_str(), (tam < DROPBOXCHUNK) ? tam : DROPBOXCHUNK, offsetForDropbox, &cabeceras);            
        string resp = util.getData();
        
        bool parsingSuccessful = reader.parse( resp, root );
        if ( !parsingSuccessful ){
             // report to the user the failure and their locations in the document.
            //Traza::print("Dropbox::chunckedUpload: Failed to parse configuration. " + reader.getFormattedErrorMessages(), W_ERROR);
            Traza::print("Dropbox::chunckedUpload: " + resp, W_ERROR);
        } else {
            upId = root.get("session_id","").asString();
            Traza::print("Obtenido un session_id: " + upId, W_DEBUG);
        }
        //DROPBOXAPIV2
        int httpCode = util.getHttp_code();
        
        offsetForDropbox += (tam < DROPBOXCHUNK) ? tam : DROPBOXCHUNK;     
        while (offsetForDropbox < tam){
            Traza::print("Subiendo %" , (offsetForDropbox / (float) tam) *100, W_DEBUG);
            Traza::print("Offset: " + Constant::TipoToStr(offsetForDropbox) 
                         + " tam: " + Constant::TipoToStr(chunkFileSize), W_DEBUG);

            cabeceras.clear();
            cabeceras.insert( make_pair("Authorization", AuthOauth2));
            cabeceras.insert( make_pair("Content-Type", "application/octet-stream"));
            cabeceras.insert( make_pair("User-Agent", "Mozilla/5.0 (Windows; U; Windows NT 5.1; en-GB; rv:1.8.1.14) Gecko/20080404 Firefox/2.0.0.14"));
            cabeceras.insert( make_pair("Accept", "*/*"));


            Json::Value postArg;
            Json::Value parmCursor;
            parmCursor["offset"] = (Json::UInt64)offsetForDropbox;
            parmCursor["session_id"]= upId.empty() ? "" : upId;
//                parmCursor["validateResponse"] = false;
//                parmCursor["file"] = cloudIdPath;
            postArg["close"] = false;
            postArg["cursor"] = parmCursor;
            Json::FastWriter fastWriter;

            std::string output = fastWriter.write(postArg);
            Traza::print(output, W_DEBUG);
            cabeceras.insert( make_pair("Dropbox-API-Arg", output));
            util.post(DROPBOXURLPUTCHUNKED, filesystemPath.c_str(), chunkFileSize, offsetForDropbox, &cabeceras);
            //httpCode = util.getHttp_code();

            Traza::print("retorno subida", httpCode, W_DEBUG);
            if (httpCode != 200){
                resp = util.getData();
                Traza::print(resp, W_DEBUG);
            }
            offsetForDropbox += DROPBOXCHUNK;
        }
        
        if (httpCode == 200) {
            chunkFileSize = tam - offsetForDropbox;
            Traza::print("FINAL. Offset: " + Constant::TipoToStr(offsetForDropbox) 
                             + " tam: " + Constant::TipoToStr(chunkFileSize), W_DEBUG);
            
            ret = commitChunkedUpload(filesystemPath, cloudIdPath, accessToken, upId, offsetForDropbox, chunkFileSize);
        }
    } else {
        Traza::print("Upload aborted. Access token empty", W_ERROR);
    }
    return ret;
}

/**
*
*/
bool Dropbox::commitChunkedUpload(string filesystemPath, string dropboxPath, string accessToken, string upId, size_t offset, size_t tam){
    string AuthOauth2 = "Bearer " + accessToken;
    map<string, string> cabeceras;
    Json::Value root;   // will contains the root value after parsing.
    Json::Reader reader;
    
    cabeceras.clear();
    cabeceras.insert( make_pair("Authorization", AuthOauth2));
    cabeceras.insert( make_pair("Content-Type", "application/octet-stream"));
    
    Json::Value postArg;
    Json::Value parmCursor;
    Json::Value partCommit;
    parmCursor["offset"] = (Json::UInt64)offset;
    parmCursor["session_id"]= upId.empty() ? "" : upId;
    partCommit["path"] = dropboxPath;
    partCommit["mode"] = "add";
    partCommit["autorename"] = false;
    partCommit["mute"] = false;    
    postArg["cursor"] = parmCursor;
    postArg["commit"] = partCommit;
    
    Json::FastWriter fastWriter;
    std::string output = fastWriter.write(postArg);
    Traza::print(output, W_DEBUG);
    cabeceras.insert( make_pair("Dropbox-API-Arg", output));
    //Envio del commit
    util.post(DROPBOXURLCOMMITCHUNKED, filesystemPath.c_str(), tam, offset, &cabeceras);
    //util.post(DROPBOXURLCOMMITCHUNKED, "", &cabeceras);
    Traza::print("retorno commit", util.getHttp_code(), W_DEBUG);
    
    std::string res = util.getData();
    if (util.getHttp_code() == 200){
        return true;
    } else {
        Traza::print("Dropbox::commitChunkedUpload. Error in commit: " + res, W_ERROR);
        return false;
    }
    
}

/**
*
*/
bool Dropbox::deleteFiles(string fileid, string accessToken){
    string url = DROPBOXURLDELETE;
    string AuthOauth2 = "Bearer " + accessToken;
    map<string, string> cabeceras;
    bool result = false;

    if (!fileid.empty()){
        cabeceras.clear();
        cabeceras.insert( make_pair("Authorization", AuthOauth2));
        cabeceras.insert( make_pair("Content-Type", "application/json"));
        Json::Value postArg;
        postArg["path"] = fileid;
        Json::FastWriter fastWriter;
        std::string output = fastWriter.write(postArg);
        Traza::print("Ruta a eliminar: " + fileid, W_DEBUG);
        util.post(url, output, &cabeceras);

        Json::Value root;   // will contains the root value after parsing.
        Json::Reader reader;
        string resp = util.getData();
        Traza::print(resp, W_DEBUG);
        bool parsingSuccessful = reader.parse( resp, root );
        if ( !parsingSuccessful ){
             // report to the user the failure and their locations in the document.
            Traza::print("Dropbox::deleteFiles: Failed to parse configuration. " + reader.getFormattedErrorMessages(), W_ERROR);
        } else {
            // Get the value of the member of root named 'hash', return '' if there is no
            // such member.
            string error = root.get("error","").asString();
            if (error.empty()){
                result = true;
            }
        }
        return result;

    } else {
        Traza::print("Delete aborted. Access token empty", W_ERROR);
        return false;
    }
}

/**
* Se baja un fichero
*/
int Dropbox::getFile(string filesystemPath, string cloudIdPath, string accessToken){
    string AuthOauth2 = "Bearer " + accessToken;
    map<string, string> cabeceras;
    Json::Value postArg;
    Json::FastWriter fastWriter;
    cabeceras.clear();
    
    cabeceras.insert( make_pair("Authorization", AuthOauth2));
    cabeceras.insert( make_pair("Content-Type", "text/plain"));
    postArg["path"] = cloudIdPath;
    std::string output = fastWriter.write(postArg);
    cabeceras.insert( make_pair("Dropbox-API-Arg", output));
    
    Traza::print(string("Descargando ") + cloudIdPath + " en " + filesystemPath, W_DEBUG);
    util.postDownload(DROPBOXURLGET, filesystemPath, &cabeceras);
    if (util.getHttp_code() != 200){
        Traza::print(string("Error descargando ") + cloudIdPath + " en " + filesystemPath, W_ERROR);
        return -1;
    }
        
    
    return 0;
}

/**
* Obtiene informacion de la ruta indicada
*/
string Dropbox::getJSONListDropbox(string filesystemPath, string accessToken){
    map<string, string> cabeceras;
    string responseMetadata;
    string AuthOauth2 = "Bearer " + accessToken;

    if (!accessToken.empty()){
        cabeceras.clear();
        cabeceras.insert( make_pair("Authorization", AuthOauth2));
        cabeceras.insert( make_pair("Content-Type", "application/json; charset=utf-8"));
        Json::Value postArg;
        postArg["path"] = filesystemPath;
        postArg["recursive"] = false;
        postArg["include_media_info"] = false;
        postArg["include_deleted"] = false;
        postArg["include_has_explicit_shared_members"] = false;
        postArg["include_mounted_folders"] = false;
        Json::FastWriter fastWriter;
        std::string output = fastWriter.write(postArg);

        util.post(DROPBOXURLLIST, output, &cabeceras);
        responseMetadata = util.getData();  
        Traza::print(responseMetadata, W_DEBUG);
    }
    return responseMetadata;
}

/**
* Obtiene el resto de elementos
*/
string Dropbox::getJSONListContinueDropbox(string cursor, string accessToken){
    map<string, string> cabeceras;
    string responseMetadata;
    string AuthOauth2 = "Bearer " + accessToken;

    if (!accessToken.empty()){
        cabeceras.clear();
        cabeceras.insert( make_pair("Authorization", AuthOauth2));
        cabeceras.insert( make_pair("Content-Type", "application/json"));
        Json::Value postArg;
        postArg["cursor"] = cursor;
        Json::FastWriter fastWriter;
        std::string output = fastWriter.write(postArg);

        util.post(DROPBOXURLLISTNEXT, output, &cabeceras);
        responseMetadata = util.getData();  
        Traza::print(responseMetadata, W_DEBUG);
    }
    return responseMetadata;
}

/**
*
*/
bool Dropbox::listFiles(string filesystemPath, string accessToken, CloudFiles *files){
    
    bool hasMore = false;
    string cursor = ""; //Campo para obtener mas registros si es necesario
    int controlBucle = 0;
    
    string responseMetadata = getJSONListDropbox(filesystemPath, accessToken);
    
    do{
        Json::Value root;   // will contains the root value after parsing.
        Json::Reader reader;
        bool parsingSuccessful = reader.parse( responseMetadata, root );

        if ( !parsingSuccessful ){
             // report to the user the failure and their locations in the document.
            Traza::print("Dropbox::listFiles. Failed to parse configuration: " + reader.getFormattedErrorMessages(), W_ERROR);
        } else {
            // Get the value of the member of root named 'hash', return '' if there is no
            // such member.
    //        files->strHash = root.get("hash","").asString();
    //        files->bytes = root.get("bytes",0).asDouble();
    //        files->path = root.get("path","").asString();
    //        files->revision = root.get("rev","0").asString();
    //        files->root = root.get("root","").asString();
    //        files->isDir = root.get("is_dir", true).asBool();
    //
    //        string out = "listFiles: " + files->strHash + ", " + Constant::TipoToStr(files->bytes)
    //                     + ", " + files->path + ", " + files->revision + ", "
    //                     + files->root + ", " + string(files->isDir ? "S":"N");
    //
    //        Traza::print(out, W_DEBUG);

            hasMore = root.get("has_more", false).asBool();
            cursor = root.get("cursor","").asString();
            const Json::Value contents = root["entries"];

            for ( int index = 0; index < contents.size(); index++ ){  // Iterates over the sequence elements.
                CloudFiles *childFile = new CloudFiles();
    //            childFile->revision = contents[index].get("rev","0").asString();
                childFile->strHash = contents[index].get("id","").asString();
    //            childFile->bytes = contents[index].get("bytes",0).asDouble();
                childFile->path = contents[index].get("path_lower","").asString();
    //            childFile->root = contents[index].get("root","").asString();
                childFile->isDir = contents[index].get(".tag","").asString() == "folder" ? true : false;
                files->fileList.push_back(childFile);
            }
            controlBucle++;
        } 
        
        if (hasMore) 
            responseMetadata = getJSONListDropbox(filesystemPath, accessToken);
        
    } while (hasMore && controlBucle < 10);
    
    return true;
}
