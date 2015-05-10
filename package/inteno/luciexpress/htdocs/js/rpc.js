//! Author: Martin K. Schröder <mkschreder.uk@gmail.com>

(function(){
	var RPC_HOST = ""; //(($config.rpc.host)?$config.rpc.host:"")
	var RPC_SESSION_ID = "00000000000000000000000000000000"; 
	var gettext = function(text){ return text; }
	function rpc_request(type, namespace, method, data){
		var sid = ""; 
		var deferred = $.Deferred(); 
		// setup default rpcs
		$.jsonRPC.withOptions({
			namespace: "", 
			endPoint: RPC_HOST+"/ubus"
		}, function(){	 
			//var sid = "00000000000000000000000000000000"; 
			//if($rootScope.sid) sid = $rootScope.sid; 
			//data.ubus_rpc_session = sid;  
			this.request(type, {
				params: [ RPC_SESSION_ID, namespace, method, data],
				success: function(result){
					//alert("SID: "+sid + " :: "+ JSON.stringify(result)); 
					if(type == "call" && result && result.result) {
						// TODO: modify all rpc UCI services so that they ALWAYS return at least 
						// an empty json object. Otherwise we have no way to differentiate success 
						// from failure of a request. This has to be done on the host side. 
						if(result.result[0] != 0){ // || result.result[1] == undefined) {
							console.log("RPC succeeded, but returned error: "+JSON.stringify(result));
							deferred.reject((function(){
								switch(result.result[0]){
									case 0: return gettext("Parse error"); 
									case 1: return gettext("Invalid request"); 
									case 2: return gettext("Invalid parameters"); 
									case 3: return gettext("Internal error"); 
									case 4: return gettext("Object not found"); 
									case 5: return gettext("Session not found"); 
									case 6: return gettext("Access denied"); 
									case 7: return gettext("Timed out"); 
									default: return gettext("RPC error #")+result.result[0]+": "+result.result[1]; 
								}
							})()); 
						} else {
							deferred.resolve(result.result[1]);
						}
					} else if(type == "list" && result && result.result){
						deferred.resolve(result.result); 
					} else {
						deferred.reject(); 
					}
				}, 
				error: function(result){
					console.error("RPC error ("+namespace+"."+method+"): "+JSON.stringify(result));
					if(result && result.error){
						deferred.reject(result.error);  
						//$rootScope.$broadcast("error", result.error.message); 
					}
				}
			})
		});
		return deferred.promise(); 
	}
	var rpc = window.rpc = {
		$sid: function(sid){
			if(sid) RPC_SESSION_ID = sid; 
			else return RPC_SESSION_ID; 
		}, 
		$register: function(call){
			//console.log("registering: "+call); 
			var self = this; 
			function _find(path, obj){
				if(!obj.hasOwnProperty(path[0])){
					obj[path[0]] = {}; 
				}
				if(path.length == 1) {
					var namespace = call.split("."); 
					namespace.pop(); namespace = namespace.join("."); 
					(function(namespace, method){
						// create the rpc method
						obj[path[0]] = function(data){
							if(!data) data = { }; 
							return rpc_request("call", namespace, method, data); 
						}
					})(namespace, path[0]); 
				} else {
					var child = path[0]; 
					path.shift(); 
					_find(path, obj[child]); 
				}
			}
			_find(call.split("."), self); 
		}, 
		$init: function(){
			var self = this; 
			var deferred = $.Deferred(); 
			// request list of all methods and construct rpc object containing all of the methods in javascript. 
			rpc_request("list", "*", "", {}).done(function(result){
				//console.log("RESULT: "+JSON.stringify(result)); 
				// TODO: make this less obscure of a method :)
				function _processNode(obj, cur_path){
					var is_leaf = true; 
					var leafs = {}; 
					Object.keys(obj).map(function(x){
						if((typeof obj[x]) == "object") {
							leafs[x] = obj[x]; 
							is_leaf = false; 
						} else {
							
						}
					}); 
					if(is_leaf){
						// add a new rpc call 
						//console.log("Leaf: "+namespace+", "+method); 
						self.$register(cur_path); 
					} else { 
						//console.log("Processing node: "+cur_path); 
						Object.keys(leafs).map(function(x){
							var path = ((cur_path)?(cur_path+"."):"")+x; 
							//var namespace = parent[x] = {}; 
							_processNode(leafs[x], path); 
						}); 
					}
				}
				_processNode(result, null); 
				deferred.resolve(); 
			}); 
			return deferred.promise(); 
		}
	}; 
})(); 

// luci rpc module for communicating with the server
angular.module("luci")
.factory('$rpc', function($rootScope, $config, gettext){
	
	return window.rpc; 
}); 
