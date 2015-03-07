var fs = require('fs');
var winston = require('winston');
var child_process = require('child_process');

winston.add(winston.transports.File, { filename: '/var/log/beaglestache.log' });

var stacheExit = function(code, signal) {
    winston.info('stache exited: ' + code + ' signal: ' + signal);
};

var stache = child_process.spawn('./stache', 
 ['-1','stache-mask.png','6','4','400','240','24'], 
 {stdio:['pipe', 'pipe', process.stderr]}
);
stache.stdout.setEncoding('ascii');
stache.on('exit', stacheExit);

// keep HDMI output turned on by unblanking the display every 30 seconds
setInterval(unblankDisplay, 30000);
function unblankDisplay() {
    try {
        fs.writeFileSync("/sys/class/graphics/fb0/blank", "0");
    } catch(ex) {
    }
};
