<template>
    <div id="network-settings" class="settings-container container-fluid box transparent">
        <h4><span class="semi-bold">Network</span> Settings</h4>    
        <br>
      
                <div class="interfaces box">
                    <div class="box-header"><h4>Available Interfaces</h4></div>
                    <div class="box-body p-t-0">
                        <form>
                        <Tabs classes="nav-tabs-simple">
                            <Tab header="ETH0 (Wired)">
                               <div class="form-group row">
                                   <div class="col-sm-7">
                                       <label for="" class="form-control-label">Interface Mode</label>
                                       <p class="text-muted hidden-sm-down">Static is a specific IP address you want the select interface to bind to. Please note this HAS to be a unique IP address on the network. Multi machines can not have the same IP and will just cause you problems. <br>DHCP is a auto assigned IP from your router. Once this IP has been assigned, as long as your router continues to see this device on your network it "SHOULD" keep that assinged IP unless you reset the DHCP settings of your router then this can change. In DHCP mode you can not assign any of the following 3 entries in the Interface section as these come from your router.</p>
                                   </div>
                                   <div class="col-sm-3 col-sm-offset-1 text-xs-right">
                                       <label class="radio-inline">
                                          <input type="radio" id="eth_interface_mode_static" value="static" v-model="eth_interface_mode"> Static
                                        </label>
                                        <label class="radio-inline">
                                          <input type="radio" id="eth_interface_mode_dhcp" value="dhcp" v-model="eth_interface_mode"> DHCP
                                        </label>
                                   </div>           
                               </div>

                               <div class="form-group row" v-if="eth_interface_mode == 'static'">
                                   <div class="col-sm-7">
                                       <label for="" class="form-control-label text-md">IP Address</label>
                                       <p class="text-muted  hidden-sm-down">Here you can enter the designated IP in the form of ###.###.###.### (Ex: 192.168.0.26). You do not need to put in leading 0's, but you do need all 4 sections seperated by a "."</p>
                                   </div>
                                   <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                       <input type="text" v-model="eth_ip_address" class="form-control">
                                   </div>
                               </div>

                               <div class="form-group row" v-if="eth_interface_mode == 'static'">
                                   <div class="col-sm-7">
                                       <label for="" class="form-control-label text-md">Netmask</label>
                                       <p class="text-muted  hidden-sm-down">Normally this is 255.255.255.0 for most setups. If your setup is unique, you will know if a different netmask is required for your network.</p>
                                   </div>
                                   <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                       <input type="text" v-model="eth_netmask" class="form-control">
                                   </div>
                               </div>

                               <div class="form-group row" v-if="eth_interface_mode == 'static'">
                                   <div class="col-sm-7">
                                       <label for="" class="form-control-label text-md">Gateway</label>
                                       <p class="text-muted  hidden-sm-down">This will typically be one of 2 different setups. Format again is in ###.###.###.### and could end in .1 or .254 for most home routers (Ex: 192.168.0.1 or 192.168.0.254). If you are not sure you can look at your network properties for your laptop or desktop to see what its gateway IP is set to.</p>
                                   </div>
                                   <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                       <input type="text" v-model="eth_gateway" class="form-control">
                                   </div>
                               </div>

                            </Tab>
                            <Tab header="WLAN0 (Wireless)">
                               <div class="form-group row">
                                   <div class="col-sm-7">
                                       <label for="" class="form-control-label">Interface Mode</label>
                                       <p class="text-muted hidden-sm-down">Static is a specific IP address you want the select interface to bind to. Please note this HAS to be a unique IP address on the network. Multi machines can not have the same IP and will just cause you problems. <br>DHCP is a auto assigned IP from your router. Once this IP has been assigned, as long as your router continues to see this device on your network it "SHOULD" keep that assinged IP unless you reset the DHCP settings of your router then this can change. In DHCP mode you can not assign any of the following 3 entries in the Interface section as these come from your router.</p>
                                   </div>
                                   <div class="col-sm-3 col-sm-offset-1 text-xs-right">
                                       <label class="radio-inline">
                                          <input type="radio" id="wlan_interface_mode_static" value="static" v-model="wlan_interface_mode"> Static
                                        </label>
                                        <label class="radio-inline">
                                          <input type="radio" id="wlan_interface_mode_dhcp" value="dhcp" v-model="wlan_interface_mode"> DHCP
                                        </label>
                                   </div>           
                               </div>

                               <div class="form-group row" v-if="wlan_interface_mode == 'static'">
                                   <div class="col-sm-7">
                                       <label for="" class="form-control-label text-md">IP Address</label>
                                       <p class="text-muted  hidden-sm-down">Here you can enter the designated IP in the form of ###.###.###.### (Ex: 192.168.0.26). You do not need to put in leading 0's, but you do need all 4 sections seperated by a "."</p>
                                   </div>
                                   <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                       <input type="text" v-model="wlan_ip_address" class="form-control">
                                   </div>
                               </div>

                               <div class="form-group row" v-if="wlan_interface_mode == 'static'">
                                   <div class="col-sm-7">
                                       <label for="" class="form-control-label text-md">Netmask</label>
                                       <p class="text-muted  hidden-sm-down">Normally this is 255.255.255.0 for most setups. If your setup is unique, you will know if a different netmask is required for your network.</p>
                                   </div>
                                   <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                       <input type="text" v-model="wlan_netmask" class="form-control">
                                   </div>
                               </div>

                               <div class="form-group row" v-if="wlan_interface_mode == 'static'">
                                   <div class="col-sm-7">
                                       <label for="" class="form-control-label text-md">Gateway</label>
                                       <p class="text-muted  hidden-sm-down">This will typically be one of 2 different setups. Format again is in ###.###.###.### and could end in .1 or .254 for most home routers (Ex: 192.168.0.1 or 192.168.0.254). If you are not sure you can look at your network properties for your laptop or desktop to see what its gateway IP is set to.</p>
                                   </div>
                                   <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                       <input type="text" v-model="wlan_gateway" class="form-control">
                                   </div>
                               </div>

                               <hr>

                               <h4 class="m-b-2"><span class="semi-bold">Wireless</span> Credentials</h4>

                               <div class="form-group row" >
                                   <div class="col-sm-7">
                                   <label for="" class="form-control-label text-md">Network ID  <small class="text-muted font-italic m-l-1">WPA SSID</small></label>
                                       <p class="text-muted  hidden-sm-down">Enter the name of the wireless network you want the player to connect to.</p>
                                   </div>
                                   <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                       <input type="text" v-model="wlan_ssid" class="form-control">
                                   </div>
                               </div>

                               <div class="form-group row" >
                                   <div class="col-sm-7">
                                   <label for="" class="form-control-label text-md">Network Password  <small class="text-muted font-italic m-l-1">WPA Pre-Shared Key (PSK)</small></label>
                                       <p class="text-muted  hidden-sm-down">Enter the password (if required) for the wireless network entered above.</p>
                                   </div>
                                   <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                       <input type="text" v-model="wlan_password" class="form-control">
                                   </div>
                               </div>

                            </Tab>
                         </Tabs>
                        </form>
                    </div>
                    <div class="box-footer clearfix">
                        <a href="#" class="button update-interface pull-sm-right">Update Interface</a>
                    </div>
                </div>
          
                <div class="dns box">
                    <div class="box-header"><h4>DNS Settings</h4></div>
                      <div class="box-body">
                         
                         <div class="form-group row">
                             <div class="col-sm-7">
                                 <label for="" class="form-control-label text-md">Hostname</label>
                                 <p class="text-muted  hidden-sm-down">This must be a single word, 8 letters or less, alpa/numeric only (no symbols, punciation or spaces) and should be unique to your setup. This lets the Falcon Player software find other compatible player devices on your network and also allows you to access that unique player by its name in your web browser. Some common examples are FPP1, FPP2 or FPPM, FPPS1, FPPS2 etc.</p>
                             </div>
                             <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                 <input type="text" v-model="hostname" class="form-control">
                             </div>
                         </div>

                         <div class="form-group row">
                             <div class="col-sm-7">
                                 <label for="" class="form-control-label">DNS Server Mode</label>
                                 <p class="text-muted hidden-sm-down">Manual or DCHP Mode</p>
                             </div>
                             <div class="col-sm-3 col-sm-offset-1 text-xs-right">
                                 <label class="radio-inline">
                                    <input type="radio" id="dns_server_mode_manual" value="manual" v-model="dns_server_mode"> Manual
                                  </label>
                                  <label class="radio-inline">
                                    <input type="radio" id="dns_server_mode_dhcp" value="dhcp" v-model="dns_server_mode"> DHCP
                                  </label>
                             </div>           
                         </div>

                         <div class="form-group row" v-if="dns_server_mode == 'manual'">
                             <div class="col-sm-7">
                                 <label for="" class="form-control-label text-md">DNS Server 1</label>
                                 <p class="text-muted  hidden-sm-down">In Static mode this is in the standard IP format of ###.###.###.### - most of the time this will be the IP address of your router. This can also be a public DNS IP as well (Ex: Google is 8.8.8.8).</p>
                             </div>
                             <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                 <input type="text" v-model="dns_server_1" class="form-control">
                             </div>
                         </div>

                         <div class="form-group row" v-if="dns_server_mode == 'manual'">
                             <div class="col-sm-7">
                                 <label for="" class="form-control-label text-md">DNS Server 2</label>
                                 <p class="text-muted  hidden-sm-down">Optional, but same setup as DNS Server 1.</p>
                             </div>
                             <div class="col-sm-4 col-sm-offset-1 text-xs-right">
                                 <input type="text" v-model="dns_server_2" class="form-control">
                             </div>
                         </div>

                      </div>
                      <div class="box-footer clearfix">
                          <a href="#" class="button update-dns pull-sm-right">Update DNS</a>
                      </div>
                </div>
             
    </div>
</template>

<script>
import Tabs from '../../../shared/tabset.vue';
import Tab from '../../../shared/tab.vue';

export default {
    components: { Tabs, Tab },
    props: [],
    data() {
        return {};
    },
    ready() {},
    methods: {},
    events: {}

}
</script>

<style lang="sass">
    @import "resources/assets/sass/_vars.scss";
    @import "resources/assets/sass/_mixins.scss";

</style>