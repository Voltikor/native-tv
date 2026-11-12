'use strict';
var assert = require('assert');
var fs = require('fs');
var path = require('path');
var vm = require('vm');
var root = path.join(__dirname, '../..');
var html = fs.readFileSync(path.join(root, 'assets/home-app/index.html'), 'utf8');
var script = html.match(/<script>([\s\S]*?)<\/script>/)[1];
assert.strictEqual(JSON.parse(fs.readFileSync(path.join(root, 'assets/home-app/appinfo.json'))).handlesRelaunch, true);

function relay(bridge, visible, foreground, fail) {
  var calls = [], timers = [], events = {}, activated = 0;
  function respond(uri, data, done) {
    calls.push({ uri: uri, data: data });
    done({ returnValue: uri !== fail, appId: foreground });
  }
  var context = {
    document: { visibilityState: visible ? 'visible' : 'hidden',
      addEventListener: function (name, fn) { events[name] = fn; } },
    window: { PalmSystem: { activate: function () { activated++; } } },
    setTimeout: function (fn) { timers.push(fn); return timers.length; },
    clearTimeout: function () {}
  };
  if (bridge) {
    context.window.PalmServiceBridge = function () {
      var self = this;
      this.call = function (uri, body) {
        respond(uri, JSON.parse(body), function (reply) { self.onservicecallback(JSON.stringify(reply)); });
      };
    };
  } else {
    context.window.webOS = { service: { request: function (uri, options) {
      assert(!/\/test$/.test(uri), 'the service name excludes the method category');
      respond(uri + '/' + options.method, options.parameters, options.onSuccess);
    } } };
  }
  vm.runInNewContext(script, context);
  return { calls: calls, timers: timers, context: context,
    relaunch: function () { events.webOSRelaunch(); },
    activated: function () { return activated; } };
}

var manager = 'luna://com.webos.applicationManager/';
var input = 'luna://com.webos.service.networkinput/test/sendKeyCode';
var app = 'com.tvweb.dashboard';
for (var bridge of [false, true]) {
  var atHome = relay(bridge, false, app);
  assert.strictEqual(atHome.calls.length, 0, 'hidden load never steals focus');
  atHome.relaunch();
  assert.deepStrictEqual(atHome.calls.map(function (c) { return c.uri; }),
    [manager + 'getForegroundAppInfo', input]);
  assert.strictEqual(atHome.calls[1].data.keyCode, 102, 'send keyboard Home instead of system Home');
  assert.strictEqual(atHome.activated(), 0, 'the redirect must stay hidden');

  var otherApp = relay(bridge, false, 'netflix');
  otherApp.relaunch();
  assert.strictEqual(otherApp.calls[1].uri, manager + 'launch');
  assert.strictEqual(otherApp.calls[1].data.id, app);
  assert.strictEqual(otherApp.calls[1].data.params.screen, 'home');
  assert.strictEqual(otherApp.activated(), 0, 'Home from streaming launches Native directly');

  var cold = relay(bridge, true, 'netflix');
  assert.strictEqual(cold.calls[0].uri, manager + 'launch', 'cold startup retains the visible launch');

  for (var failure of [input, manager + 'getForegroundAppInfo', manager + 'launch']) {
    var broken = relay(bridge, false, failure === manager + 'launch' ? 'netflix' : app, failure);
    broken.relaunch();
    assert.strictEqual(broken.activated(), 1, 'a refused call falls back to the visible redirect');
    var count = broken.calls.length;
    broken.timers.forEach(function (fn) { fn(); });
    assert.strictEqual(broken.calls.length, count, 'hidden retries must not steal focus later');
  }
}
console.log('Home relay checks passed (webOS.service and PalmServiceBridge)');
