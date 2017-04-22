var HOST = "http://example.com/";

function noop(callback) {
    return callback || function() { };
}

function pad(num, size) {
    return ("000000000" + num).substr(-size)
}

function send(action, callback) {
    $.get(HOST + $("#secret").val() + action, noop(callback));
}

function main() {
    $("#beeper").click(function() {
        send("beeper", function(result) {
            var prev = "";
            var curr = "";
        
            result.split("\n").forEach(function(e) {
                var num = parseInt(e.replace("<", ""), 10);
                
                if(Number.isNaN(num)) {
                    return;
                }
                
                num = Math.floor(num / 1000);
                
                if(e.indexOf("<") >= 0 || prev.length > 0) {
                    prev = num + " " + prev;
                }
                else {
                    curr = num + " " + curr;
                }
            });
            
            $("#beeper-data").text(curr + prev);
        });
    });
    
    $("#keyboard").click(function() {
        var a = $("#keyboard-mod-one").val();
        var b = $("#keyboard-mod-two").val();
        var c = $("#keyboard-key").val();
        
        send("keyboard" + pad(parseInt(a, 10) + parseInt(b, 10), 3) + c);
    });
    
    $("#status").click(function() {
        send("status", function(result) {
            $("#status-data").val(result);
        });
    });
    
    $("#secret").change(function() {
        location.hash = $(this).val();
    });
    
    $("#reset").click(function() {
        send("reset");
    });
    
    $("#power-short").click(function() {
        send("power");
    });
    
    $("#power-long").click(function() {
        send("force");
    });
    
    $("#secret").val(location.hash.substr(1));
}

if (navigator.standalone) {
    $(window).bind("touchmove", function(e) {
        e.preventDefault();
    });
}

$(document).ready(main);